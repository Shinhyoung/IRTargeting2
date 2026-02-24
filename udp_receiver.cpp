/*
 * UDP Coordinate Receiver
 *
 * IRViewer로부터 UDP로 전송된 x,y 좌표를 수신하여
 * 1024x768 캔버스에 실시간으로 시각화하는 프로그램.
 *
 * - 수신 포트: 7777
 * - 패킷 포맷: "x1,y1;x2,y2;..." (세미콜론으로 복수 좌표 구분)
 * - 'q' 키: 종료
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>      // UDP 소켓
#include <ws2tcpip.h>

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <chrono>
#include <utility>

// ===== 설정 =====
constexpr int UDP_PORT     = 7777;
constexpr int CANVAS_W     = 1024;
constexpr int CANVAS_H     = 768;
constexpr int PANEL_H      = 130;   // 하단 정보 패널 높이
constexpr int TRAIL_FRAMES = 40;    // 좌표 trail 길이 (프레임 수)

using PointList = std::vector<std::pair<int, int>>;

// ===== 그리드 그리기 =====
static void drawGrid(cv::Mat& img)
{
    const cv::Scalar gridColor(35, 38, 45);
    const cv::Scalar borderColor(70, 75, 90);

    // 10x10 그리드
    for (int i = 1; i < 10; i++)
    {
        int gx = i * CANVAS_W / 10;
        int gy = i * CANVAS_H / 10;
        cv::line(img, {gx, 0},       {gx, CANVAS_H}, gridColor, 1);
        cv::line(img, {0,  gy},      {CANVAS_W, gy}, gridColor, 1);
    }

    // 테두리
    cv::rectangle(img, cv::Rect(0, 0, CANVAS_W, CANVAS_H), borderColor, 2);

    // 축 라벨
    const cv::Scalar labelColor(70, 70, 80);
    cv::putText(img, "0,0",        {5, 14},              cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, "1023,0",     {CANVAS_W - 60, 14},  cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, "0,767",      {5, CANVAS_H - 4},    cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, "1023,767",   {CANVAS_W - 72, CANVAS_H - 4}, cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
}

// ===== Trail 그리기 (오래된 것일수록 어둡게 페이드) =====
static void drawTrail(cv::Mat& img, const std::deque<PointList>& history)
{
    for (int fi = (int)history.size() - 1; fi >= 1; fi--)
    {
        float alpha  = 1.0f - (float)fi / TRAIL_FRAMES;
        int   a      = (int)(alpha * 200);
        int   radius = 3 + (int)(alpha * 3);

        for (const auto& [px, py] : history[fi])
        {
            if (px < 0 || px >= CANVAS_W || py < 0 || py >= CANVAS_H) continue;
            cv::circle(img, {px, py}, radius, cv::Scalar(0, a / 3, a), -1);
        }
    }
}

// ===== 현재 포인트 강조 그리기 =====
static void drawCurrentPoints(cv::Mat& img, const PointList& pts)
{
    for (const auto& [px, py] : pts)
    {
        if (px < 0 || px >= CANVAS_W || py < 0 || py >= CANVAS_H) continue;

        // 외곽 링 (두 겹)
        cv::circle(img, {px, py}, 22, cv::Scalar(0,  80, 200), 1);
        cv::circle(img, {px, py}, 14, cv::Scalar(0, 160, 255), 2);
        // 중심 원
        cv::circle(img, {px, py},  6, cv::Scalar(0, 230, 255), -1);

        // 십자선
        cv::line(img, {px - 20, py}, {px + 20, py}, cv::Scalar(0, 180, 255), 1);
        cv::line(img, {px, py - 20}, {px, py + 20}, cv::Scalar(0, 180, 255), 1);

        // 좌표 텍스트
        std::string coord = "(" + std::to_string(px) + ", " + std::to_string(py) + ")";
        cv::putText(img, coord, {px + 16, py - 12},
                    cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(0, 255, 180), 1);
    }
}

int main()
{
    // ===== Winsock 초기화 =====
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
        return -1;
    }

    SOCKET recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (recvSocket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;   // 모든 IP에서 수신

    if (bind(recvSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed on port " << UDP_PORT
                  << ". Error: " << WSAGetLastError() << std::endl;
        closesocket(recvSocket);
        WSACleanup();
        return -1;
    }

    // Non-blocking 모드 (Tick 루프에서 폴링)
    u_long nonBlocking = 1;
    ioctlsocket(recvSocket, FIONBIO, &nonBlocking);

    std::cout << "=== UDP Coordinate Receiver ===" << std::endl;
    std::cout << "Listening on port " << UDP_PORT << " ..." << std::endl;
    std::cout << "Press 'q' in the window to quit." << std::endl;

    // ===== OpenCV 윈도우 =====
    const std::string winName = "UDP Receiver  [ Port: 7777 ]  |  IRViewer <-> Unreal";
    cv::namedWindow(winName, cv::WINDOW_AUTOSIZE);

    // ===== 상태 변수 =====
    std::deque<PointList> history;       // 최근 TRAIL_FRAMES 프레임의 포인트 리스트
    std::string lastRawMsg   = "";
    int         totalPackets = 0;
    auto        lastRecvTime = std::chrono::steady_clock::now();
    bool        connected    = false;

    // ===== 메인 루프 =====
    while (true)
    {
        // ----- UDP 수신 -----
        char        buf[2048];
        sockaddr_in senderAddr{};
        int         senderLen = sizeof(senderAddr);

        int bytes = recvfrom(recvSocket, buf, sizeof(buf) - 1, 0,
                             reinterpret_cast<sockaddr*>(&senderAddr), &senderLen);

        if (bytes > 0)
        {
            buf[bytes] = '\0';
            lastRawMsg = std::string(buf);
            totalPackets++;
            connected    = true;
            lastRecvTime = std::chrono::steady_clock::now();

            // 파싱: "x1,y1;x2,y2;..."
            PointList currentPoints;
            std::istringstream ss(lastRawMsg);
            std::string token;
            while (std::getline(ss, token, ';'))
            {
                size_t comma = token.find(',');
                if (comma == std::string::npos) continue;
                try
                {
                    int x = std::stoi(token.substr(0, comma));
                    int y = std::stoi(token.substr(comma + 1));
                    currentPoints.emplace_back(x, y);
                }
                catch (...) {}
            }

            if (!currentPoints.empty())
            {
                history.push_front(currentPoints);
                if ((int)history.size() > TRAIL_FRAMES)
                    history.pop_back();
            }
        }

        // 3초 이상 수신 없으면 연결 끊김으로 표시
        auto  now         = std::chrono::steady_clock::now();
        float sinceRecv   = std::chrono::duration<float>(now - lastRecvTime).count();
        if (sinceRecv > 3.0f) connected = false;

        // ----- 렌더링 -----
        // 전체 캔버스 (좌표 영역 + 하단 패널)
        cv::Mat canvas(CANVAS_H + PANEL_H, CANVAS_W, CV_8UC3, cv::Scalar(15, 17, 22));

        // 좌표 영역 배경
        cv::rectangle(canvas, cv::Rect(0, 0, CANVAS_W, CANVAS_H),
                      cv::Scalar(20, 23, 30), -1);

        drawGrid(canvas);
        drawTrail(canvas, history);

        if (!history.empty())
            drawCurrentPoints(canvas, history.front());

        // ----- 하단 정보 패널 -----
        const int py = CANVAS_H + 12;

        // 연결 상태 LED
        cv::Scalar statusColor = connected ? cv::Scalar(0, 255, 80) : cv::Scalar(50, 50, 200);
        std::string statusStr  = connected ? "CONNECTED" : "WAITING...";
        cv::circle(canvas, {18, py + 13}, 7, statusColor, -1);
        cv::putText(canvas, statusStr, {32, py + 18},
                    cv::FONT_HERSHEY_SIMPLEX, 0.60, statusColor, 2);

        // 포트 / 패킷 수
        cv::putText(canvas, "Port: 7777",
                    {200, py + 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(160, 160, 160), 1);
        cv::putText(canvas, "Packets received: " + std::to_string(totalPackets),
                    {330, py + 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(160, 160, 160), 1);

        // 마지막 수신 raw 데이터
        std::string lastDisp = lastRawMsg.empty() ? "(none)" : lastRawMsg;
        if (lastDisp.size() > 60) lastDisp = lastDisp.substr(0, 60) + "...";
        cv::putText(canvas, "Last packet: \"" + lastDisp + "\"",
                    {10, py + 48}, cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(0, 210, 210), 1);

        // 현재 포인트 수 & 좌표 목록
        int curCount = history.empty() ? 0 : (int)history.front().size();
        cv::putText(canvas, "Points: " + std::to_string(curCount),
                    {10, py + 80}, cv::FONT_HERSHEY_SIMPLEX, 0.62, cv::Scalar(220, 190, 0), 1);

        if (!history.empty())
        {
            std::string allCoords;
            for (size_t i = 0; i < history.front().size(); i++)
            {
                if (i > 0) allCoords += "   |   ";
                allCoords += "#" + std::to_string(i + 1) + ": ("
                           + std::to_string(history.front()[i].first) + ", "
                           + std::to_string(history.front()[i].second) + ")";
            }
            cv::putText(canvas, allCoords,
                        {10, py + 108}, cv::FONT_HERSHEY_SIMPLEX, 0.58,
                        cv::Scalar(80, 255, 120), 1);
        }

        cv::putText(canvas, "q: quit",
                    {CANVAS_W - 75, py + 108}, cv::FONT_HERSHEY_SIMPLEX,
                    0.48, cv::Scalar(90, 90, 90), 1);

        cv::imshow(winName, canvas);

        if (cv::waitKey(1) == 'q') break;
    }

    cv::destroyAllWindows();
    closesocket(recvSocket);
    WSACleanup();

    std::cout << "Receiver terminated. Total packets received: " << std::endl;
    return 0;
}
