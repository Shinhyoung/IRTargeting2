/*
 * OptiTrack Flex 13 카메라 IR 영상 추적 프로그램
 *
 * 기능:
 * - OptiTrack Camera SDK를 사용하여 Flex 13 카메라에서 IR 영상 획득
 * - Grayscale 모드로 원본 IR 이미지 캡처 (IR LED 비활성화)
 * - Morphological dilation을 통한 노이즈 제거 및 객체 강조
 * - 이진화(Threshold)를 통한 밝은 객체 검출
 * - 마우스 클릭으로 관심 영역(ROI) 선택 및 호모그래피 변환
 * - 시작/런타임 설정 다이얼로그 (IP, Port, 해상도, 노출)
 */

// Winsock2는 반드시 Windows.h 이전에 포함해야 함
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

#include "settings.h"
#include "homography.h"
#include "udp_sender.h"
#include "frame_processor.h"
#include "osd_renderer.h"

#include <cstdint>
#include "cameralibrary.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace CameraLibrary;

int main(int argc, char* argv[])
{
    // ========== 로그 파일 설정 ==========
    std::ofstream logFile("IRViewer_log.txt");
    auto cout_buf = std::cout.rdbuf(logFile.rdbuf());
    auto cerr_buf = std::cerr.rdbuf(logFile.rdbuf());

    // 중복 정리 코드 통합: 오류 종료 시 restoreLog() 호출
    auto restoreLog = [&]()
    {
        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();
    };

    std::cout << "=== OptiTrack Flex 13 Camera IR Viewer (Camera SDK) ===" << std::endl;
    std::cout << "Log started at: "
              << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

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
        restoreLog();
        MessageBoxA(NULL, "Failed to initialize Camera SDK. Check IRViewer_log.txt for details.",
                    "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    std::cout << "Camera SDK initialized successfully." << std::endl;

    CameraList list;
    std::cout << "Number of cameras detected: " << list.Count() << std::endl;

    if (list.Count() == 0)
    {
        std::cerr << "No cameras found!" << std::endl;
        restoreLog();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "No OptiTrack cameras found. Check IRViewer_log.txt for details.",
                    "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    for (int i = 0; i < list.Count(); i++)
    {
        std::cout << "Camera " << i << ": " << list[i].Name() << std::endl;
        std::cout << "  UID: " << list[i].UID() << std::endl;
        std::cout << "  Initial State: " << list[i].State() << std::endl;
    }

    std::cout << "Waiting for camera to fully initialize..." << std::endl;
    bool cameraReady = false;
    for (int i = 0; i < 100; i++) // 최대 10초 대기
    {
        CameraList cur;
        if (cur.Count() > 0 && cur[0].State() == 6)
        {
            cameraReady = true;
            std::cout << "Camera initialized! (State: " << cur[0].State() << ")" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!cameraReady)
    {
        std::cerr << "Camera failed to initialize within 10 seconds." << std::endl;
        restoreLog();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Camera initialization timeout. Check IRViewer_log.txt for details.",
                    "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Getting camera with UID: " << list[0].UID() << std::endl;
    std::shared_ptr<Camera> camera = CameraManager::X().GetCamera(list[0].UID());

    if (!camera)
    {
        std::cerr << "Failed to get camera pointer." << std::endl;
        restoreLog();
        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Failed to get camera pointer. Check IRViewer_log.txt for details.",
                    "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Camera Serial: " << camera->Serial() << std::endl;
    std::cout << "Camera Name: "   << camera->Name()   << std::endl;
    std::cout << "Camera Resolution: " << camera->Width() << "x" << camera->Height() << std::endl;

    // ========== 카메라 설정 ==========
    camera->SetVideoType(Core::GrayscaleMode);
    std::cout << "Camera set to Grayscale mode." << std::endl;

    camera->SetExposure(settings.exposure);
    std::cout << "Exposure set to " << settings.exposure << "." << std::endl;

    camera->SetIntensity(0);
    std::cout << "IR illumination disabled (intensity set to 0)." << std::endl;

    camera->Start();
    std::cout << "Camera started." << std::endl;

    // ========== OpenCV 윈도우 & 마우스 콜백 ==========
    int frameWidth  = camera->Width();
    int frameHeight = camera->Height();

    std::string windowName = "OptiTrack Flex 13 - IR View";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE | cv::WINDOW_GUI_NORMAL);

    HomographyState hom;
    static MouseCallbackData mouseData;
    mouseData.windowName   = windowName;
    mouseData.frameWidth   = frameWidth;
    mouseData.frameHeight  = frameHeight;
    mouseData.targetWidth  = settings.targetWidth;
    mouseData.targetHeight = settings.targetHeight;
    mouseData.state        = &hom;
    cv::setMouseCallback(windowName, onMouse, &mouseData);

    std::cout << "Instructions:" << std::endl;
    std::cout << "  [Q/ESC] Quit  [S] UDP toggle  [R] Reset  [P] Settings" << std::endl;
    std::cout << "  Left-click on LEFT image to select 4 corner points." << std::endl;

    // ========== UDP 초기화 ==========
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
        CameraManager::X().Shutdown();
        return -1;
    }

    UDPSender sender;
    if (!sender.init(settings.ipAddress, settings.port))
    {
        WSACleanup();
        CameraManager::X().Shutdown();
        return -1;
    }
    std::cout << "UDP socket ready. Target: " << settings.ipAddress << ":" << settings.port << std::endl;
    std::cout << "Press 's' to send current detected coordinates via UDP." << std::endl;

    // ========== 메인 루프 ==========
    bool running        = true;
    bool continuousSend = false;
    std::vector<cv::Point2f> latestSendCenters; // 마지막으로 검출된 전송 대상 좌표

    while (running)
    {
        std::shared_ptr<const Frame> frame = camera->LatestFrame();

        if (frame && frame->IsGrayscale())
        {
            const unsigned char* data = frame->GrayscaleData(*camera);
            if (data)
            {
                FrameResult r = processFrame(data, frameWidth, frameHeight, hom, settings);

                // 전송 대상 좌표 갱신
                latestSendCenters = hom.ready ? r.inBoundCenters : std::vector<cv::Point2f>{};

                // 좌우 패널 합성
                cv::Mat combined;
                cv::hconcat(r.leftPanel, r.rightPanel, combined);

                // OSD 렌더링
                OSDState osd;
                osd.continuousSend    = continuousSend;
                osd.homographyReady   = hom.ready;
                osd.selectedPointCount = static_cast<int>(hom.selectedPoints.size());
                osd.displayCount      = hom.ready
                                       ? static_cast<int>(r.inBoundCenters.size())
                                       : static_cast<int>(r.detectedCenters.size());
                renderOSD(combined, osd);

                cv::imshow(windowName, combined);
            }
        }

        // ===== 연속 UDP 전송 (호모그래피 설정 완료 후에만) =====
        if (continuousSend && hom.ready && !latestSendCenters.empty())
            sender.send(latestSendCenters);

        // ===== 키 입력 처리 =====
        int key = cv::waitKey(1);

        if (key == 'q' || key == 'Q' || key == 27)
        {
            running = false;
        }
        else if (key == 'r' || key == 'R')
        {
            hom.reset();
            latestSendCenters.clear();
            std::cout << "Point selection reset." << std::endl;
        }
        else if (key == 's' || key == 'S')
        {
            continuousSend = !continuousSend;
            std::cout << "[UDP] Real-time send: " << (continuousSend ? "ON" : "OFF") << std::endl;
        }
        else if (key == 'p' || key == 'P')
        {
            AppSettings prev = settings;
            if (ShowSettingsDialog(settings))
            {
                if (settings.exposure != prev.exposure)
                {
                    camera->SetExposure(settings.exposure);
                    std::cout << "[Settings] Exposure updated to " << settings.exposure << std::endl;
                }
                if (strcmp(settings.ipAddress, prev.ipAddress) != 0 ||
                    settings.port != prev.port)
                {
                    sender.updateTarget(settings.ipAddress, settings.port);
                }
                if (settings.targetWidth  != prev.targetWidth ||
                    settings.targetHeight != prev.targetHeight)
                {
                    mouseData.targetWidth  = settings.targetWidth;
                    mouseData.targetHeight = settings.targetHeight;
                    hom.reset();
                    latestSendCenters.clear();
                    std::cout << "[Settings] Target resolution changed to "
                              << settings.targetWidth << "x" << settings.targetHeight
                              << ". Homography reset." << std::endl;
                }
            }
        }
    }

    // ========== 정리 및 종료 ==========
    cv::destroyAllWindows();
    WSACleanup();
    CameraManager::X().Shutdown();

    std::cout << "Program terminated successfully." << std::endl;
    restoreLog();
    return 0;
}
