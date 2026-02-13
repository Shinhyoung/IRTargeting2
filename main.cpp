/*
 * OptiTrack Flex 13 카메라 IR 영상 추적 프로그램
 *
 * 기능:
 * - OptiTrack Camera SDK를 사용하여 Flex 13 카메라에서 IR 영상 획득
 * - Grayscale 모드로 원본 IR 이미지 캡처 (IR LED 비활성화)
 * - Morphological dilation을 통한 노이즈 제거 및 객체 강조
 * - 이진화(Threshold)를 통한 밝은 객체 검출
 * - 움직이는 객체만 추적하여 좌표 표시 (정적인 객체 필터링)
 * - OpenCV를 사용한 실시간 영상 표시 및 시각화
 */

#include <cstdint>
#include "cameralibrary.h"      // OptiTrack Camera SDK 헤더
#include <opencv2/opencv.hpp>   // OpenCV 라이브러리 (영상 처리 및 표시)
#include <iostream>              // 표준 입출력
#include <fstream>               // 파일 입출력 (로그 파일)
#include <memory>                // 스마트 포인터 (shared_ptr)
#include <chrono>                // 시간 측정
#include <thread>                // 스레드 및 sleep 기능
#include <Windows.h>             // Windows API (MessageBox)

using namespace CameraLibrary;

int main(int argc, char* argv[])
{
    // ========== 로그 파일 설정 ==========
    // 디버깅 및 문제 해결을 위해 모든 출력을 로그 파일로 리다이렉션
    std::ofstream logFile("IRViewer_log.txt");
    auto cout_buf = std::cout.rdbuf(logFile.rdbuf());  // 표준 출력을 파일로 리다이렉션
    auto cerr_buf = std::cerr.rdbuf(logFile.rdbuf());  // 표준 에러를 파일로 리다이렉션

    std::cout << "=== OptiTrack Flex 13 Camera IR Viewer (Camera SDK) ===" << std::endl;
    std::cout << "Log started at: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

    // ========== Camera SDK 초기화 ==========
    // OptiTrack Camera SDK를 초기화하고 연결된 카메라를 검색
    std::cout << "Initializing Camera SDK..." << std::endl;
    CameraManager::X().WaitForInitialization();  // SDK 초기화 대기

    // 카메라 초기화 성공 여부 확인
    if (!CameraManager::X().AreCamerasInitialized())
    {
        std::cerr << "Failed to initialize cameras." << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "1. Camera is connected via USB" << std::endl;
        std::cerr << "2. Camera drivers are installed" << std::endl;
        std::cerr << "3. No other software (Motive) is using the camera" << std::endl;

        // 로그 파일 정리 및 에러 메시지 표시
        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();

        MessageBoxA(NULL, "Failed to initialize Camera SDK. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Camera SDK initialized successfully." << std::endl;

    // ========== 카메라 검색 및 확인 ==========
    // 연결된 OptiTrack 카메라 목록 가져오기
    CameraList list;
    std::cout << "Number of cameras detected: " << list.Count() << std::endl;

    // 카메라가 하나도 없으면 에러 처리
    if (list.Count() == 0)
    {
        std::cerr << "No cameras found!" << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "1. Camera is connected via USB" << std::endl;
        std::cerr << "2. Camera drivers are installed" << std::endl;
        std::cerr << "3. No other software is using the camera" << std::endl;

        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();

        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "No OptiTrack cameras found. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // ========== 카메라 정보 출력 ==========
    // 검색된 모든 카메라의 정보를 로그에 기록
    for (int i = 0; i < list.Count(); i++)
    {
        std::cout << "Camera " << i << ": " << list[i].Name() << std::endl;
        std::cout << "  UID: " << list[i].UID() << std::endl;
        std::cout << "  Initial State: " << list[i].State() << std::endl;
    }

    // ========== 카메라 초기화 대기 ==========
    // 카메라가 완전히 초기화될 때까지 대기 (State 6 = Initialized)
    // USB 카메라는 초기화에 시간이 걸릴 수 있음
    std::cout << "Waiting for camera to fully initialize..." << std::endl;
    const int maxWaitSeconds = 10;  // 최대 10초 대기
    bool cameraReady = false;

    // 100ms 간격으로 카메라 상태 확인 (최대 10초)
    for (int i = 0; i < maxWaitSeconds * 10; i++)
    {
        CameraList currentList;
        if (currentList.Count() > 0 && currentList[0].State() == 6) // 6 = Initialized
        {
            cameraReady = true;
            std::cout << "Camera initialized! (State: " << currentList[0].State() << ")" << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 초기화 타임아웃 체크
    if (!cameraReady)
    {
        std::cerr << "Camera failed to initialize within " << maxWaitSeconds << " seconds." << std::endl;

        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();

        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Camera initialization timeout. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // ========== 카메라 객체 가져오기 ==========
    // 첫 번째 카메라의 UID를 사용하여 카메라 객체 획득
    std::cout << "Getting camera with UID: " << list[0].UID() << std::endl;
    std::shared_ptr<Camera> camera = CameraManager::X().GetCamera(list[0].UID());

    if (!camera)
    {
        std::cerr << "Failed to get camera pointer." << std::endl;

        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();

        CameraManager::X().Shutdown();
        MessageBoxA(NULL, "Failed to get camera pointer. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    // 카메라 정보 출력
    std::cout << "Camera Serial: " << camera->Serial() << std::endl;
    std::cout << "Camera Name: " << camera->Name() << std::endl;
    std::cout << "Camera Resolution: " << camera->Width() << "x" << camera->Height() << std::endl;

    // ========== 카메라 설정 ==========
    // Grayscale 모드로 설정하여 원본 IR 영상 획득
    // (다른 모드: SegmentMode, ObjectMode, MJPEGMode 등)
    std::cout << "Setting camera to Grayscale mode..." << std::endl;
    camera->SetVideoType(Core::GrayscaleMode);
    std::cout << "Camera set to Grayscale mode." << std::endl;

    // IR LED 조명 비활성화 (intensity를 0으로 설정)
    // 외부 IR 광원만 사용하여 순수한 IR 반사 영상 획득
    std::cout << "Disabling IR illumination..." << std::endl;
    camera->SetIntensity(0);
    std::cout << "IR illumination disabled (intensity set to 0)." << std::endl;

    // ========== 카메라 시작 ==========
    // 카메라 프레임 캡처 시작
    std::cout << "Starting camera..." << std::endl;
    camera->Start();
    std::cout << "Camera started. Press 'q' to quit." << std::endl;

    // ========== OpenCV 윈도우 생성 ==========
    // 영상 표시를 위한 OpenCV 윈도우 생성
    std::string windowName = "OptiTrack Flex 13 - IR View";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    // 프레임 크기 저장
    int frameWidth = camera->Width();
    int frameHeight = camera->Height();

    // ========== 모션 감지 변수 ==========
    // 이전 프레임의 중심점들을 저장하여 움직임 감지
    std::vector<cv::Point> previousCenters;
    const int MOTION_THRESHOLD = 10;  // 10픽셀 이상 움직여야 "움직이는 객체"로 판단

    // ========== 메인 루프 ==========
    bool running = true;
    while (running)
    {
        // ===== 프레임 획득 =====
        // 카메라로부터 최신 프레임 가져오기
        std::shared_ptr<const Frame> frame = camera->LatestFrame();

        // 프레임이 존재하고 Grayscale 타입인지 확인
        if (frame && frame->IsGrayscale())
        {
            // ===== Grayscale 데이터 추출 =====
            // 프레임으로부터 원시 grayscale 데이터 포인터 획득
            const unsigned char* grayscaleData = frame->GrayscaleData(*camera);

            if (grayscaleData)
            {
                // ===== OpenCV Mat 생성 =====
                // 카메라 데이터를 OpenCV Mat 객체로 변환
                // CV_8UC1: 8bit unsigned, 1 channel (grayscale)
                cv::Mat grayFrame(frameHeight, frameWidth, CV_8UC1, (void*)grayscaleData);

                // ===== Morphological Dilation 적용 =====
                // 팽창 연산을 통해 밝은 영역 확대 및 노이즈 제거
                // 3x3 사각형 커널을 사용하여 3번 반복 적용
                cv::Mat dilatedFrame;
                cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
                cv::dilate(grayFrame, dilatedFrame, kernel, cv::Point(-1, -1), 3);

                // ===== 이진화 (Binary Threshold) =====
                // 밝기 200을 기준으로 이진화
                // 200 이상: 255 (흰색), 200 미만: 0 (검은색)
                // 이를 통해 밝은 객체만 추출
                cv::Mat binaryFrame;
                cv::threshold(dilatedFrame, binaryFrame, 200, 255, cv::THRESH_BINARY);

                // ===== 윤곽선 검출 =====
                // 이진 영상에서 객체의 윤곽선(contours) 찾기
                // RETR_EXTERNAL: 가장 바깥쪽 윤곽선만 검출
                // CHAIN_APPROX_SIMPLE: 윤곽선을 압축하여 저장 (메모리 절약)
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(binaryFrame.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                // ===== 표시용 컬러 이미지 생성 =====
                // Grayscale 이미지를 BGR 컬러 이미지로 변환 (색상 표시를 위해)
                cv::Mat dilatedDisplay, binaryDisplay;
                cv::cvtColor(dilatedFrame, dilatedDisplay, cv::COLOR_GRAY2BGR);
                cv::cvtColor(binaryFrame, binaryDisplay, cv::COLOR_GRAY2BGR);

                // ===== 중심점 계산 및 표시 =====
                // 현재 프레임에서 검출된 중심점들을 저장
                std::vector<cv::Point> currentCenters;

                // 각 윤곽선에 대해 중심점 계산
                for (const auto& contour : contours)
                {
                    // ===== Image Moments 계산 =====
                    // Moments를 사용하여 객체의 중심 좌표 계산
                    // m10/m00 = 중심 x좌표, m01/m00 = 중심 y좌표
                    cv::Moments m = cv::moments(contour);

                    if (m.m00 > 0) // 0으로 나누기 방지
                    {
                        // ===== 중심 좌표 계산 =====
                        // 질량 중심(centroid) 계산
                        int cx = static_cast<int>(m.m10 / m.m00);
                        int cy = static_cast<int>(m.m01 / m.m00);
                        cv::Point center(cx, cy);
                        currentCenters.push_back(center);

                        // ===== 움직임 감지 =====
                        // 이전 프레임의 중심점들과 비교하여 움직임 판단
                        // 10픽셀 이상 이동했으면 "움직이는 객체"로 판단
                        bool isMoving = true;
                        for (const auto& prevCenter : previousCenters)
                        {
                            // 유클리드 거리 계산
                            double distance = cv::norm(center - prevCenter);
                            if (distance < MOTION_THRESHOLD)
                            {
                                // 이전 위치와 10픽셀 이내 = 정적인 객체
                                isMoving = false;
                                break;
                            }
                        }

                        // ===== 움직이는 객체만 표시 =====
                        // 정적인 객체는 표시하지 않음 (배경 노이즈 제거)
                        if (isMoving)
                        {
                            // 중심점에 빨간색 원 그리기
                            // Scalar(B, G, R) - OpenCV는 BGR 순서 사용
                            cv::circle(binaryDisplay, center, 5, cv::Scalar(0, 0, 255), -1);

                            // 좌표 텍스트를 초록색으로 표시
                            std::string coordText = "(" + std::to_string(cx) + "," + std::to_string(cy) + ")";
                            cv::putText(binaryDisplay, coordText, cv::Point(cx + 10, cy - 5),
                                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
                        }
                    }
                }

                // ===== 이전 프레임 정보 업데이트 =====
                // 다음 프레임의 움직임 감지를 위해 현재 중심점들 저장
                previousCenters = currentCenters;

                // ===== 이미지 합성 =====
                // 왼쪽: Dilate 적용 후 grayscale 영상
                // 오른쪽: 이진화 후 움직이는 객체 표시
                cv::Mat combined;
                cv::hconcat(dilatedDisplay, binaryDisplay, combined);

                // ===== 화면에 표시 =====
                cv::imshow(windowName, combined);
            }
        }

        // ===== 키 입력 처리 =====
        // 1ms 동안 키 입력 대기 (OpenCV는 이 시간 동안 이벤트 처리)
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) // 'q', 'Q', 또는 ESC 키
        {
            running = false;
        }

        // ===== CPU 사용률 제한 =====
        // 10ms 대기하여 과도한 CPU 사용 방지
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ========== 정리 및 종료 ==========
    // OpenCV 윈도우 닫기
    cv::destroyAllWindows();

    // Camera SDK 종료 및 리소스 해제
    CameraManager::X().Shutdown();

    std::cout << "Program terminated successfully." << std::endl;

    // 표준 입출력 스트림 복원
    std::cout.rdbuf(cout_buf);
    std::cerr.rdbuf(cerr_buf);
    logFile.close();

    return 0;
}
