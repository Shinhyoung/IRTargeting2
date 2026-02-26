/*
 * UDP Coordinate Receiver
 *
 * IRViewer로부터 UDP로 전송된 좌표를 수신하여 캔버스에 실시간 시각화.
 *
 * 패킷 포맷: "x1,y1;x2,y2;..." (세미콜론으로 복수 좌표 구분)
 *
 * - 수신 포트: 7777 (기본값)
 * - 시작 시 해상도 설정 창 표시 (기본값: 1024x768)
 * - 'q' 키: 종료
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <chrono>
#include <utility>
#include <algorithm>

// ===== 고정 설정 =====
constexpr int UDP_PORT     = 7777;
constexpr int PANEL_H      = 130;
constexpr int TRAIL_FRAMES = 40;

using PointList = std::vector<std::pair<int, int>>;

// ========== 해상도 설정 다이얼로그 ==========
#define IDC_RES_WIDTH  301
#define IDC_RES_HEIGHT 302

static HWND  g_hResW = nullptr, g_hResH = nullptr;
static int   g_resW  = 1024, g_resH = 768;
static int   g_resDlgResult = 0;

LRESULT CALLBACK ResDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        const int margin = 12, labelW = 130, editW = 100, editH = 22, rowH = 32;
        int y = margin;

        auto makeLabel = [&](LPCSTR text, int yy)
        {
            HWND h = CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE,
                margin, yy + 3, labelW, 18, hwnd, nullptr,
                GetModuleHandleA(nullptr), nullptr);
            SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        };
        auto makeEdit = [&](LPCSTR text, int id, int yy) -> HWND
        {
            HWND h = CreateWindowA("EDIT", text,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
                margin + labelW + 8, yy, editW, editH, hwnd,
                (HMENU)(UINT_PTR)id, GetModuleHandleA(nullptr), nullptr);
            SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
            return h;
        };

        char buf[32];

        makeLabel("Canvas Width (px):", y);
        sprintf_s(buf, "%d", g_resW);
        g_hResW = makeEdit(buf, IDC_RES_WIDTH, y); y += rowH;

        makeLabel("Canvas Height (px):", y);
        sprintf_s(buf, "%d", g_resH);
        g_hResH = makeEdit(buf, IDC_RES_HEIGHT, y); y += rowH + 10;

        const int btnW = 80, btnH = 28, btnGap = 12;
        const int clientW = margin * 2 + labelW + 8 + editW;
        int startX = (clientW - btnW * 2 - btnGap) / 2;

        HWND hOk = CreateWindowA("BUTTON", "OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            startX, y, btnW, btnH, hwnd, (HMENU)IDOK,
            GetModuleHandleA(nullptr), nullptr);
        HWND hCancel = CreateWindowA("BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX + btnW + btnGap, y, btnW, btnH, hwnd, (HMENU)IDCANCEL,
            GetModuleHandleA(nullptr), nullptr);
        SendMessage(hOk,     WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            char buf[32];
            GetWindowTextA(g_hResW, buf, sizeof(buf));
            int w = atoi(buf);
            if (w > 0) g_resW = w;

            GetWindowTextA(g_hResH, buf, sizeof(buf));
            int h = atoi(buf);
            if (h > 0) g_resH = h;

            g_resDlgResult = 1;
            DestroyWindow(hwnd);
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            g_resDlgResult = 0;
            DestroyWindow(hwnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(g_resDlgResult);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// true = OK, false = Cancel/닫기
bool ShowResolutionDialog()
{
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSA wc     = {};
        wc.lpfnWndProc   = ResDlgProc;
        wc.hInstance     = GetModuleHandleA(nullptr);
        wc.lpszClassName = "UDPReceiverResDlg";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassA(&wc);
        classRegistered = true;
    }

    // clientW = 12+130+8+100+12 = 262, clientH = 12+2*32+10+28+12 = 126
    RECT rc = { 0, 0, 262, 126 };
    DWORD style   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW;
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    int dlgW = rc.right  - rc.left;
    int dlgH = rc.bottom - rc.top;
    int x    = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
    int y    = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;

    g_resDlgResult = 0;
    HWND hwnd = CreateWindowExA(
        exStyle, "UDPReceiverResDlg", "UDP Receiver - Canvas Resolution",
        style, x, y, dlgW, dlgH,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG dlgMsg = {};
    while (GetMessage(&dlgMsg, nullptr, 0, 0) > 0)
    {
        if (!IsWindow(hwnd) || !IsDialogMessage(hwnd, &dlgMsg))
        {
            TranslateMessage(&dlgMsg);
            DispatchMessage(&dlgMsg);
        }
    }
    return static_cast<int>(dlgMsg.wParam) == 1;
}

// ===== 그리드 그리기 =====
static void drawGrid(cv::Mat& img, int cW, int cH)
{
    const cv::Scalar gridColor(35, 38, 45);
    const cv::Scalar borderColor(70, 75, 90);

    for (int i = 1; i < 10; i++)
    {
        int gx = i * cW / 10;
        int gy = i * cH / 10;
        cv::line(img, {gx, 0},  {gx, cH}, gridColor, 1);
        cv::line(img, {0,  gy}, {cW, gy}, gridColor, 1);
    }

    cv::rectangle(img, cv::Rect(0, 0, cW, cH), borderColor, 2);

    const cv::Scalar labelColor(70, 70, 80);
    std::string maxX = std::to_string(cW - 1);
    std::string maxY = std::to_string(cH - 1);
    cv::putText(img, "0,0",                {5, 14},          cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, maxX + ",0",          {cW - 52, 14},    cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, "0," + maxY,          {5, cH - 4},      cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
    cv::putText(img, maxX + "," + maxY,    {cW - 64, cH - 4},cv::FONT_HERSHEY_SIMPLEX, 0.38, labelColor, 1);
}

// ===== Trail 그리기 =====
static void drawTrail(cv::Mat& img, const std::deque<PointList>& history, int cW, int cH)
{
    for (int fi = (int)history.size() - 1; fi >= 1; fi--)
    {
        float alpha  = 1.0f - (float)fi / TRAIL_FRAMES;
        int   a      = (int)(alpha * 200);
        int   radius = 3 + (int)(alpha * 3);

        for (const auto& [px, py] : history[fi])
        {
            if (px < 0 || px >= cW || py < 0 || py >= cH) continue;
            cv::circle(img, {px, py}, radius, cv::Scalar(0, a / 3, a), -1);
        }
    }
}

// ===== 현재 포인트 강조 그리기 =====
static void drawCurrentPoints(cv::Mat& img, const PointList& pts, int cW, int cH)
{
    for (const auto& [px, py] : pts)
    {
        if (px < 0 || px >= cW || py < 0 || py >= cH) continue;

        cv::circle(img, {px, py}, 22, cv::Scalar(0,  80, 200), 1);
        cv::circle(img, {px, py}, 14, cv::Scalar(0, 160, 255), 2);
        cv::circle(img, {px, py},  6, cv::Scalar(0, 230, 255), -1);

        cv::line(img, {px - 20, py}, {px + 20, py}, cv::Scalar(0, 180, 255), 1);
        cv::line(img, {px, py - 20}, {px, py + 20}, cv::Scalar(0, 180, 255), 1);

        std::string coord = "(" + std::to_string(px) + ", " + std::to_string(py) + ")";
        cv::putText(img, coord, {px + 16, py - 12},
                    cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(0, 255, 180), 1);
    }
}

int main()
{
    // ===== 해상도 설정 다이얼로그 =====
    ShowResolutionDialog(); // Cancel이면 기본값(1024x768) 유지
    const int canvasW = g_resW;
    const int canvasH = g_resH;

    std::cout << "=== UDP Coordinate Receiver ===" << std::endl;
    std::cout << "Canvas: " << canvasW << "x" << canvasH << std::endl;
    std::cout << "Listening on port " << UDP_PORT << " ..." << std::endl;

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
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(recvSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed on port " << UDP_PORT
                  << ". Error: " << WSAGetLastError() << std::endl;
        closesocket(recvSocket);
        WSACleanup();
        return -1;
    }

    u_long nonBlocking = 1;
    ioctlsocket(recvSocket, FIONBIO, &nonBlocking);

    // ===== OpenCV 윈도우 =====
    const std::string winName = "UDP Receiver  [ Port: 7777 ]  |  IRViewer <-> Unreal";
    cv::namedWindow(winName, cv::WINDOW_AUTOSIZE);

    // ===== 상태 변수 =====
    std::deque<PointList> history;
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

        // 3초 이상 수신 없으면 연결 끊김
        auto  now       = std::chrono::steady_clock::now();
        float sinceRecv = std::chrono::duration<float>(now - lastRecvTime).count();
        if (sinceRecv > 3.0f) connected = false;

        // ----- 렌더링 -----
        cv::Mat canvas(canvasH + PANEL_H, canvasW, CV_8UC3, cv::Scalar(15, 17, 22));

        cv::rectangle(canvas, cv::Rect(0, 0, canvasW, canvasH),
                      cv::Scalar(20, 23, 30), -1);

        drawGrid(canvas, canvasW, canvasH);
        drawTrail(canvas, history, canvasW, canvasH);

        if (!history.empty())
            drawCurrentPoints(canvas, history.front(), canvasW, canvasH);

        // ----- 하단 정보 패널 -----
        const int py = canvasH + 12;

        cv::Scalar statusColor = connected ? cv::Scalar(0, 255, 80) : cv::Scalar(50, 50, 200);
        std::string statusStr  = connected ? "CONNECTED" : "WAITING...";
        cv::circle(canvas, {18, py + 13}, 7, statusColor, -1);
        cv::putText(canvas, statusStr, {32, py + 18},
                    cv::FONT_HERSHEY_SIMPLEX, 0.60, statusColor, 2);

        cv::putText(canvas, "Port: " + std::to_string(UDP_PORT),
                    {200, py + 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(160, 160, 160), 1);
        cv::putText(canvas, std::to_string(canvasW) + "x" + std::to_string(canvasH),
                    {320, py + 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(100, 200, 255), 1);
        cv::putText(canvas, "Packets: " + std::to_string(totalPackets),
                    {430, py + 18}, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(160, 160, 160), 1);

        std::string lastDisp = lastRawMsg.empty() ? "(none)" : lastRawMsg;
        if (lastDisp.size() > 60) lastDisp = lastDisp.substr(0, 60) + "...";
        cv::putText(canvas, "Last packet: \"" + lastDisp + "\"",
                    {10, py + 48}, cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(0, 210, 210), 1);

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
                    {canvasW - 75, py + 108}, cv::FONT_HERSHEY_SIMPLEX,
                    0.48, cv::Scalar(90, 90, 90), 1);

        cv::imshow(winName, canvas);

        if (cv::waitKey(1) == 'q') break;
    }

    cv::destroyAllWindows();
    closesocket(recvSocket);
    WSACleanup();

    std::cout << "Receiver terminated. Total packets received: " << totalPackets << std::endl;
    return 0;
}
