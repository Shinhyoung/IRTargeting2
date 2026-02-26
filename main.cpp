/*
 * OptiTrack Flex 13 카메라 IR 영상 추적 프로그램
 *
 * 기능:
 * - OptiTrack Camera SDK를 사용하여 Flex 13 카메라에서 IR 영상 획득
 * - Grayscale 모드로 원본 IR 이미지 캡처 (IR LED 비활성화)
 * - Morphological dilation을 통한 노이즈 제거 및 객체 강조
 * - 이진화(Threshold)를 통한 밝은 객체 검출
 * - 마우스 클릭으로 관심 영역(ROI) 선택 및 호모그래피 변환
 * - OpenCV를 사용한 실시간 영상 표시 및 시각화
 * - 시작/런타임 설정 다이얼로그 (IP, Port, 해상도, 노출)
 */

// Winsock2는 반드시 Windows.h 이전에 포함해야 함
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include "cameralibrary.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <memory>
#include <chrono>
#include <thread>
#include <Windows.h>

using namespace CameraLibrary;

// ========== 앱 설정 구조체 ==========
struct AppSettings
{
    char ipAddress[64];
    int  port;
    int  targetWidth;
    int  targetHeight;
    int  exposure;

    AppSettings()
    {
        strcpy_s(ipAddress, sizeof(ipAddress), "127.0.0.1");
        port         = 7777;
        targetWidth  = 1024;
        targetHeight = 768;
        exposure     = 7500;
    }
};

// ========== 설정 다이얼로그 ==========
#define IDC_SETT_IP       201
#define IDC_SETT_PORT     202
#define IDC_SETT_WIDTH    203
#define IDC_SETT_HEIGHT   204
#define IDC_SETT_EXPOSURE 205

static HWND g_hIp, g_hPort, g_hWidth, g_hHeight, g_hExposure;
static AppSettings* g_pDlgSettings = nullptr;
static int g_dlgResult = 0; // 0 = Cancel/닫기, 1 = OK

LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_pDlgSettings = (AppSettings*)((CREATESTRUCT*)lParam)->lpCreateParams;
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        const int margin = 12, labelW = 170, editW = 150, editH = 22, rowH = 32;
        int y = margin;

        auto makeLabel = [&](LPCSTR text, int yy)
        {
            HWND h = CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE,
                margin, yy + 3, labelW, 18, hwnd, nullptr,
                GetModuleHandleA(nullptr), nullptr);
            SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        };

        auto makeEdit = [&](LPCSTR text, int id, int yy, bool numOnly) -> HWND
        {
            DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
            if (numOnly) style |= ES_NUMBER;
            HWND h = CreateWindowA("EDIT", text, style,
                margin + labelW + 8, yy, editW, editH, hwnd,
                (HMENU)(UINT_PTR)id, GetModuleHandleA(nullptr), nullptr);
            SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
            return h;
        };

        char buf[64];

        makeLabel("IP Address:", y);
        g_hIp = makeEdit(g_pDlgSettings->ipAddress, IDC_SETT_IP, y, false); y += rowH;

        makeLabel("Port:", y);
        sprintf_s(buf, "%d", g_pDlgSettings->port);
        g_hPort = makeEdit(buf, IDC_SETT_PORT, y, true); y += rowH;

        makeLabel("Target Width (px):", y);
        sprintf_s(buf, "%d", g_pDlgSettings->targetWidth);
        g_hWidth = makeEdit(buf, IDC_SETT_WIDTH, y, true); y += rowH;

        makeLabel("Target Height (px):", y);
        sprintf_s(buf, "%d", g_pDlgSettings->targetHeight);
        g_hHeight = makeEdit(buf, IDC_SETT_HEIGHT, y, true); y += rowH;

        makeLabel("Exposure (0 ~ 7500):", y);
        sprintf_s(buf, "%d", g_pDlgSettings->exposure);
        g_hExposure = makeEdit(buf, IDC_SETT_EXPOSURE, y, true); y += rowH + 10;

        // OK / Cancel 버튼
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
            char buf[64];

            GetWindowTextA(g_hIp, g_pDlgSettings->ipAddress,
                           sizeof(g_pDlgSettings->ipAddress));

            GetWindowTextA(g_hPort, buf, sizeof(buf));
            int p = atoi(buf);
            if (p > 0 && p <= 65535) g_pDlgSettings->port = p;

            GetWindowTextA(g_hWidth, buf, sizeof(buf));
            int w = atoi(buf);
            if (w > 0) g_pDlgSettings->targetWidth = w;

            GetWindowTextA(g_hHeight, buf, sizeof(buf));
            int h = atoi(buf);
            if (h > 0) g_pDlgSettings->targetHeight = h;

            GetWindowTextA(g_hExposure, buf, sizeof(buf));
            int e = atoi(buf);
            g_pDlgSettings->exposure = std::max(0, std::min(7500, e));

            g_dlgResult = 1;
            DestroyWindow(hwnd);
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            g_dlgResult = 0;
            DestroyWindow(hwnd);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(g_dlgResult);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// true = OK 눌림 / false = Cancel 또는 창 닫기
bool ShowSettingsDialog(AppSettings& settings)
{
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSA wc     = {};
        wc.lpfnWndProc   = SettingsDlgProc;
        wc.hInstance     = GetModuleHandleA(nullptr);
        wc.lpszClassName = "IRViewerSettingsDlg";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
        RegisterClassA(&wc);
        classRegistered = true;
    }

    // 클라이언트 영역 크기 계산 (margin=12, labelW=170, editW=150, gap=8)
    // clientW = 12+170+8+150+12 = 352
    // clientH = 12 + 5*32 + 10 + 28 + 12 = 222
    RECT rc = { 0, 0, 352, 222 };
    DWORD style   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU; // WS_THICKFRAME 없음 = 리사이징 불가
    DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW;
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    int dlgW = rc.right  - rc.left;
    int dlgH = rc.bottom - rc.top;
    int x    = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
    int y    = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;

    g_dlgResult = 0;
    HWND hwnd = CreateWindowExA(
        exStyle, "IRViewerSettingsDlg", "IRViewer - Settings",
        style, x, y, dlgW, dlgH,
        nullptr, nullptr, GetModuleHandleA(nullptr), &settings);
    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG dlgMsg = {};
    while (GetMessage(&dlgMsg, nullptr, 0, 0) > 0)
    {
        // hwnd가 이미 파괴된 경우 IsDialogMessage 호출 방지
        if (!IsWindow(hwnd) || !IsDialogMessage(hwnd, &dlgMsg))
        {
            TranslateMessage(&dlgMsg);
            DispatchMessage(&dlgMsg);
        }
    }
    // dlgMsg.wParam = PostQuitMessage에 넘긴 값 (1=OK, 0=Cancel)
    return static_cast<int>(dlgMsg.wParam) == 1;
}

// ========== 전역 변수: 호모그래피를 위한 점 선택 ==========
std::vector<cv::Point2f> selectedPoints;
const int REQUIRED_POINTS = 4;
bool      homographyReady  = false;
cv::Mat   homographyMatrix;

// ========== 마우스 콜백용 데이터 구조체 ==========
struct MouseCallbackData
{
    std::string windowName;
    int frameWidth;
    int frameHeight;
    int targetWidth;   // 호모그래피 목표 해상도 너비
    int targetHeight;  // 호모그래피 목표 해상도 높이
};

// ========== UDP 전송용 최신 좌표 ==========
std::vector<cv::Point2f> latestSendCenters;

// ========== 마우스 콜백 함수 ==========
void onMouse(int event, int x, int y, int flags, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;

    MouseCallbackData* md = static_cast<MouseCallbackData*>(userdata);

    HWND hwnd = FindWindowA(NULL, md->windowName.c_str());
    if (!hwnd) return;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right;
    int clientH = rc.bottom;
    if (clientW <= 0 || clientH <= 0) return;

    int imgW = md->frameWidth * 2;
    int imgH = md->frameHeight;

    // WINDOW_AUTOSIZE: scale=1, offset=0이 되지만 범용적으로 letterbox 계산 유지
    float scaleX = static_cast<float>(clientW) / imgW;
    float scaleY = static_cast<float>(clientH) / imgH;
    float scale  = std::min(scaleX, scaleY);

    int dispW = static_cast<int>(imgW * scale);
    int dispH = static_cast<int>(imgH * scale);
    int offX  = (clientW - dispW) / 2;
    int offY  = (clientH - dispH) / 2;

    int localX = x - offX;
    int localY = y - offY;
    if (localX < 0 || localY < 0 || localX >= dispW || localY >= dispH) return;

    float imgX = static_cast<float>(localX) / dispW * imgW;
    float imgY = static_cast<float>(localY) / dispH * imgH;

    if (imgX < md->frameWidth && static_cast<int>(selectedPoints.size()) < REQUIRED_POINTS)
    {
        selectedPoints.push_back(cv::Point2f(imgX, imgY));
        std::cout << "Point " << selectedPoints.size() << " selected (actual): ("
                  << static_cast<int>(imgX) << ", " << static_cast<int>(imgY) << ")" << std::endl;

        if (static_cast<int>(selectedPoints.size()) == REQUIRED_POINTS)
        {
            std::cout << "All 4 points selected. Calculating homography..." << std::endl;

            float tw = static_cast<float>(md->targetWidth  - 1);
            float th = static_cast<float>(md->targetHeight - 1);
            std::vector<cv::Point2f> dstPoints = {
                {0.f, 0.f}, {tw, 0.f}, {tw, th}, {0.f, th}
            };
            homographyMatrix = cv::getPerspectiveTransform(selectedPoints, dstPoints);
            homographyReady  = true;

            std::cout << "Homography matrix calculated. Warped view ready." << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    // ========== 로그 파일 설정 ==========
    std::ofstream logFile("IRViewer_log.txt");
    auto cout_buf = std::cout.rdbuf(logFile.rdbuf());
    auto cerr_buf = std::cerr.rdbuf(logFile.rdbuf());

    std::cout << "=== OptiTrack Flex 13 Camera IR Viewer (Camera SDK) ===" << std::endl;
    std::cout << "Log started at: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

    // ========== 시작 설정 다이얼로그 ==========
    AppSettings settings;
    ShowSettingsDialog(settings); // Cancel이면 기본값 유지
    std::cout << "Settings applied: IP=" << settings.ipAddress
              << " Port=" << settings.port
              << " TargetW=" << settings.targetWidth
              << " TargetH=" << settings.targetHeight
              << " Exposure=" << settings.exposure << std::endl;

    // ========== Camera SDK 초기화 ==========
    std::cout << "Initializing Camera SDK..." << std::endl;
    CameraManager::X().WaitForInitialization();

    if (!CameraManager::X().AreCamerasInitialized())
    {
        std::cerr << "Failed to initialize cameras." << std::endl;
        std::cout.rdbuf(cout_buf); std::cerr.rdbuf(cerr_buf); logFile.close();
        MessageBoxA(NULL, "Failed to initialize Camera SDK. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    std::cout << "Camera SDK initialized successfully." << std::endl;

    CameraList list;
    std::cout << "Number of cameras detected: " << list.Count() << std::endl;

    if (list.Count() == 0)
    {
        std::cerr << "No cameras found!" << std::endl;
        std::cout.rdbuf(cout_buf); std::cerr.rdbuf(cerr_buf); logFile.close();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "No OptiTrack cameras found. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    for (int i = 0; i < list.Count(); i++)
    {
        std::cout << "Camera " << i << ": " << list[i].Name() << std::endl;
        std::cout << "  UID: " << list[i].UID() << std::endl;
        std::cout << "  Initial State: " << list[i].State() << std::endl;
    }

    std::cout << "Waiting for camera to fully initialize..." << std::endl;
    const int maxWaitSeconds = 10;
    bool cameraReady = false;

    for (int i = 0; i < maxWaitSeconds * 10; i++)
    {
        CameraList currentList;
        if (currentList.Count() > 0 && currentList[0].State() == 6)
        {
            cameraReady = true;
            std::cout << "Camera initialized! (State: " << currentList[0].State() << ")" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!cameraReady)
    {
        std::cerr << "Camera failed to initialize within " << maxWaitSeconds << " seconds." << std::endl;
        std::cout.rdbuf(cout_buf); std::cerr.rdbuf(cerr_buf); logFile.close();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Camera initialization timeout. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Getting camera with UID: " << list[0].UID() << std::endl;
    std::shared_ptr<Camera> camera = CameraManager::X().GetCamera(list[0].UID());

    if (!camera)
    {
        std::cerr << "Failed to get camera pointer." << std::endl;
        std::cout.rdbuf(cout_buf); std::cerr.rdbuf(cerr_buf); logFile.close();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Failed to get camera pointer. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Camera Serial: " << camera->Serial() << std::endl;
    std::cout << "Camera Name: " << camera->Name() << std::endl;
    std::cout << "Camera Resolution: " << camera->Width() << "x" << camera->Height() << std::endl;

    // ========== 카메라 설정 (settings 적용) ==========
    camera->SetVideoType(Core::GrayscaleMode);
    std::cout << "Camera set to Grayscale mode." << std::endl;

    camera->SetExposure(settings.exposure);
    std::cout << "Exposure set to " << settings.exposure << "." << std::endl;

    camera->SetIntensity(0);
    std::cout << "IR illumination disabled (intensity set to 0)." << std::endl;

    camera->Start();
    std::cout << "Camera started." << std::endl;

    // ========== OpenCV 윈도우 생성 ==========
    int frameWidth  = camera->Width();
    int frameHeight = camera->Height();

    std::string windowName = "OptiTrack Flex 13 - IR View";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE | cv::WINDOW_GUI_NORMAL);

    // 마우스 콜백 등록
    static MouseCallbackData mouseData;
    mouseData.windowName  = windowName;
    mouseData.frameWidth  = frameWidth;
    mouseData.frameHeight = frameHeight;
    mouseData.targetWidth  = settings.targetWidth;
    mouseData.targetHeight = settings.targetHeight;
    cv::setMouseCallback(windowName, onMouse, &mouseData);

    std::cout << "Instructions:" << std::endl;
    std::cout << "  [Q/ESC] Quit  [S] UDP toggle  [R] Reset  [P] Settings" << std::endl;
    std::cout << "  Left-click on LEFT image to select 4 corner points." << std::endl;

    // ========== UDP 소켓 초기화 ==========
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
        CameraManager::X().Shutdown();
        return -1;
    }

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET)
    {
        std::cerr << "UDP socket creation failed. Error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        CameraManager::X().Shutdown();
        return -1;
    }

    sockaddr_in udpAddr;
    // UDP 주소 적용 람다 (settings 변경 시 재사용)
    auto applyUdpSettings = [&]()
    {
        memset(&udpAddr, 0, sizeof(udpAddr));
        udpAddr.sin_family = AF_INET;
        udpAddr.sin_port   = htons(static_cast<u_short>(settings.port));
        inet_pton(AF_INET, settings.ipAddress, &udpAddr.sin_addr);
        std::cout << "UDP target: " << settings.ipAddress << ":" << settings.port << std::endl;
    };
    applyUdpSettings();

    // ========== 메인 루프 ==========
    bool running        = true;
    bool continuousSend = false;

    while (running)
    {
        // ===== 프레임 획득 =====
        std::shared_ptr<const Frame> frame = camera->LatestFrame();

        if (frame && frame->IsGrayscale())
        {
            const unsigned char* grayscaleData = frame->GrayscaleData(*camera);

            if (grayscaleData)
            {
                cv::Mat grayFrame(frameHeight, frameWidth, CV_8UC1, (void*)grayscaleData);

                // Morphological Dilation
                cv::Mat dilatedFrame;
                cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
                cv::dilate(grayFrame, dilatedFrame, kernel, cv::Point(-1, -1), 3);

                // Binary Threshold
                cv::Mat binaryFrame;
                cv::threshold(dilatedFrame, binaryFrame, 200, 255, cv::THRESH_BINARY);

                // 윤곽선 검출
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(binaryFrame.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                // 표시용 컬러 이미지
                cv::Mat grayDisplay, binaryDisplay;
                cv::cvtColor(grayFrame, grayDisplay, cv::COLOR_GRAY2BGR);
                cv::cvtColor(binaryFrame, binaryDisplay, cv::COLOR_GRAY2BGR);

                // 중심점 계산
                std::vector<cv::Point2f> detectedCenters;
                for (const auto& contour : contours)
                {
                    cv::Moments m = cv::moments(contour);
                    if (m.m00 > 0)
                    {
                        int cx = static_cast<int>(m.m10 / m.m00);
                        int cy = static_cast<int>(m.m01 / m.m00);
                        cv::Point center(cx, cy);
                        detectedCenters.push_back(cv::Point2f(cx, cy));

                        cv::circle(binaryDisplay, center, 5, cv::Scalar(0, 0, 255), -1);
                        std::string coordText = "(" + std::to_string(cx) + "," + std::to_string(cy) + ")";
                        cv::putText(binaryDisplay, coordText, cv::Point(cx + 10, cy - 5),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
                    }
                }

                latestSendCenters = detectedCenters;

                // 선택된 점 표시
                for (size_t i = 0; i < selectedPoints.size(); i++)
                {
                    cv::circle(grayDisplay, selectedPoints[i], 8, cv::Scalar(255, 0, 0), -1);
                    cv::putText(grayDisplay, std::to_string(i + 1),
                                cv::Point(selectedPoints[i].x + 10, selectedPoints[i].y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
                }

                if (selectedPoints.size() >= 2)
                {
                    for (size_t i = 0; i < selectedPoints.size() - 1; i++)
                        cv::line(grayDisplay, selectedPoints[i], selectedPoints[i + 1],
                                 cv::Scalar(0, 255, 255), 2);
                    if (selectedPoints.size() == 4)
                        cv::line(grayDisplay, selectedPoints[3], selectedPoints[0],
                                 cv::Scalar(0, 255, 255), 2);
                }

                // 오른쪽 패널: 호모그래피 준비 전 = Binary, 후 = Warped
                cv::Mat rightPanel;
                if (homographyReady)
                {
                    cv::Mat warpedImage;
                    cv::warpPerspective(grayFrame, warpedImage, homographyMatrix,
                                        cv::Size(settings.targetWidth, settings.targetHeight));
                    cv::Mat warpedColor;
                    cv::cvtColor(warpedImage, warpedColor, cv::COLOR_GRAY2BGR);

                    if (!detectedCenters.empty())
                    {
                        std::vector<cv::Point2f> transformedCenters;
                        cv::perspectiveTransform(detectedCenters, transformedCenters, homographyMatrix);

                        // 4점 영역 내 포인트만 수집 → 표시 + 전송에 사용
                        std::vector<cv::Point2f> inBoundCenters;
                        for (size_t i = 0; i < transformedCenters.size(); i++)
                        {
                            if (transformedCenters[i].x >= 0 &&
                                transformedCenters[i].x < settings.targetWidth &&
                                transformedCenters[i].y >= 0 &&
                                transformedCenters[i].y < settings.targetHeight)
                            {
                                inBoundCenters.push_back(transformedCenters[i]);
                                cv::circle(warpedColor, transformedCenters[i], 5, cv::Scalar(0, 0, 255), -1);
                                int tx = static_cast<int>(transformedCenters[i].x);
                                int ty = static_cast<int>(transformedCenters[i].y);
                                std::string coordText = "(" + std::to_string(tx) + "," + std::to_string(ty) + ")";
                                cv::putText(warpedColor, coordText, cv::Point(tx + 10, ty - 5),
                                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
                            }
                        }
                        latestSendCenters = inBoundCenters;
                    }

                    cv::resize(warpedColor, rightPanel, cv::Size(frameWidth, frameHeight));
                }
                else
                {
                    rightPanel = binaryDisplay;
                }

                cv::Mat combined;
                cv::hconcat(grayDisplay, rightPanel, combined);

                // ===== OSD: 단축키 안내 =====
                {
                    int boxH = (!selectedPoints.empty() && !homographyReady) ? 120 : 102;
                    cv::Mat overlay = combined.clone();
                    cv::rectangle(overlay, cv::Point(4, 4), cv::Point(252, boxH),
                                  cv::Scalar(15, 15, 15), cv::FILLED);
                    cv::addWeighted(overlay, 0.65, combined, 0.35, 0, combined);

                    auto putKey = [&](const std::string& t, cv::Scalar color, int y)
                    {
                        cv::putText(combined, t, cv::Point(10, y),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                                    cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
                        cv::putText(combined, t, cv::Point(10, y),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.42,
                                    color, 1, cv::LINE_AA);
                    };

                    std::string sendLabel = std::string("[S] UDP: ") +
                                            (continuousSend ? "ON  (Sending)" : "OFF");
                    cv::Scalar sendColor  = continuousSend
                                           ? cv::Scalar(60, 255, 60)
                                           : cv::Scalar(120, 200, 255);

                    putKey("[Q/ESC] Quit",                   cv::Scalar(200,200,200), 22);
                    putKey(sendLabel,                         sendColor,               40);
                    putKey("[R] Reset Corner Points",         cv::Scalar(200,200,200), 58);
                    putKey("[P] Settings",                    cv::Scalar(200,200,200), 76);
                    putKey("[L-Click] Select Corner (4pts)",  cv::Scalar(200,200,200), 94);

                    if (!selectedPoints.empty() && !homographyReady)
                        putKey("  -> " + std::to_string(selectedPoints.size()) + "/4 pts selected",
                               cv::Scalar(255, 190, 60), 112);
                }

                // ===== OSD: 하단 상태 =====
                {
                    std::string statusStr = continuousSend ? "● UDP SENDING" : "○ UDP STOPPED";
                    cv::Scalar statusColor = continuousSend
                                            ? cv::Scalar(60, 255, 60)
                                            : cv::Scalar(120, 120, 120);
                    cv::putText(combined, statusStr,
                                cv::Point(8, combined.rows - 8),
                                cv::FONT_HERSHEY_SIMPLEX, 0.50, statusColor, 1, cv::LINE_AA);

                    if (!latestSendCenters.empty())
                    {
                        std::string ptStr = "Detected: " + std::to_string(latestSendCenters.size()) + " pt(s)";
                        cv::putText(combined, ptStr,
                                    cv::Point(combined.cols - 190, combined.rows - 8),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(0, 220, 220), 1, cv::LINE_AA);
                    }
                }

                cv::imshow(windowName, combined);
            }
        }

        // ===== 연속 UDP 전송 (호모그래피 적용 후 변환 좌표만 전송) =====
        if (continuousSend && homographyReady && !latestSendCenters.empty())
        {
            // 패킷 포맷: "x1,y1;x2,y2;..."
            std::string msg;
            for (size_t i = 0; i < latestSendCenters.size(); i++)
            {
                if (i > 0) msg += ";";
                msg += std::to_string(static_cast<int>(latestSendCenters[i].x))
                     + ","
                     + std::to_string(static_cast<int>(latestSendCenters[i].y));
            }
            sendto(udpSocket, msg.c_str(), static_cast<int>(msg.length()),
                   0, reinterpret_cast<sockaddr*>(&udpAddr), sizeof(udpAddr));
        }

        // ===== 키 입력 처리 =====
        int key = cv::waitKey(1);

        if (key == 'q' || key == 'Q' || key == 27)
        {
            running = false;
        }
        else if (key == 'r' || key == 'R')
        {
            selectedPoints.clear();
            homographyReady = false;
            std::cout << "Point selection reset." << std::endl;
        }
        else if (key == 's' || key == 'S')
        {
            continuousSend = !continuousSend;
            std::cout << "[UDP] Real-time send: " << (continuousSend ? "ON" : "OFF") << std::endl;
        }
        else if (key == 'p' || key == 'P')
        {
            // ===== 런타임 설정 변경 =====
            AppSettings prev = settings;
            if (ShowSettingsDialog(settings))
            {
                // Exposure 즉시 적용
                if (settings.exposure != prev.exposure)
                {
                    camera->SetExposure(settings.exposure);
                    std::cout << "[Settings] Exposure updated to " << settings.exposure << std::endl;
                }
                // UDP 주소/포트 즉시 적용
                if (strcmp(settings.ipAddress, prev.ipAddress) != 0 ||
                    settings.port != prev.port)
                {
                    applyUdpSettings();
                }
                // 타깃 해상도 변경 시 호모그래피 리셋
                if (settings.targetWidth  != prev.targetWidth ||
                    settings.targetHeight != prev.targetHeight)
                {
                    mouseData.targetWidth  = settings.targetWidth;
                    mouseData.targetHeight = settings.targetHeight;
                    selectedPoints.clear();
                    homographyReady = false;
                    std::cout << "[Settings] Target resolution changed to "
                              << settings.targetWidth << "x" << settings.targetHeight
                              << ". Homography reset." << std::endl;
                }
            }
        }

    }

    // ========== 정리 및 종료 ==========
    cv::destroyAllWindows();
    closesocket(udpSocket);
    WSACleanup();
    CameraManager::X().Shutdown();

    std::cout << "Program terminated successfully." << std::endl;
    std::cout.rdbuf(cout_buf);
    std::cerr.rdbuf(cerr_buf);
    logFile.close();

    return 0;
}
