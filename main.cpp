/*
 * OptiTrack Flex 13 카메라 IR 영상 추적 프로그램
 *
 * 기능:
 * - OptiTrack Camera SDK를 사용하여 Flex 13 카메라에서 IR 영상 획득
 * - Grayscale 모드로 원본 IR 이미지 캡처 (IR LED 비활성화)
 * - Morphological dilation을 통한 노이즈 제거 및 객체 강조
 * - 이진화(Threshold)를 통한 밝은 객체 검출
 * - 움직이는 객체만 추적하여 좌표 표시 (정적인 객체 필터링)
 * - 마우스 클릭으로 관심 영역(ROI) 선택 및 호모그래피 변환
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

// ========== 전역 변수: 호모그래피를 위한 점 선택 ==========
std::vector<cv::Point2f> selectedPoints;  // 사용자가 선택한 4개의 점
const int REQUIRED_POINTS = 4;            // 필요한 점의 개수
bool homographyReady = false;              // 호모그래피 준비 완료 여부
cv::Mat homographyMatrix;                  // 호모그래피 변환 행렬
int frameWidthGlobal = 0;                  // 프레임 너비 (마우스 콜백에서 사용)

// ========== 마우스 콜백 함수 ==========
// 사용자가 마우스로 클릭한 점을 저장
// 왼쪽 영상(grayscale)에서만 클릭을 인식
void onMouse(int event, int x, int y, int flags, void* userdata)
{
    if (event == cv::EVENT_LBUTTONDOWN)
    {
        // 왼쪽 영상 영역인지 확인 (합쳐진 이미지의 왼쪽 절반)
        if (x < frameWidthGlobal && selectedPoints.size() < REQUIRED_POINTS)
        {
            // 클릭한 점 저장
            selectedPoints.push_back(cv::Point2f(x, y));
            std::cout << "Point " << selectedPoints.size() << " selected: ("
                      << x << ", " << y << ")" << std::endl;

            // 4개 점이 모두 선택되면 호모그래피 계산
            if (selectedPoints.size() == REQUIRED_POINTS)
            {
                std::cout << "All 4 points selected. Calculating homography..." << std::endl;

                // 목표 좌표: 1024x768 사각형의 네 모서리
                // lefttop, righttop, rightbottom, leftbottom 순서
                std::vector<cv::Point2f> dstPoints;
                dstPoints.push_back(cv::Point2f(0, 0));           // lefttop
                dstPoints.push_back(cv::Point2f(1023, 0));        // righttop
                dstPoints.push_back(cv::Point2f(1023, 767));      // rightbottom
                dstPoints.push_back(cv::Point2f(0, 767));         // leftbottom

                // 호모그래피 변환 행렬 계산
                homographyMatrix = cv::getPerspectiveTransform(selectedPoints, dstPoints);
                homographyReady = true;

                std::cout << "Homography matrix calculated. Warped view ready." << std::endl;
            }
        }
    }
}

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

    // 노출(Exposure) 설정
    // 값이 클수록 더 밝은 영상, 작을수록 어두운 영상
    // 범위: 일반적으로 1~480 (카메라 모델에 따라 다름)
    std::cout << "Setting exposure to 200..." << std::endl;
    camera->SetExposure(200);
    std::cout << "Exposure set to 200." << std::endl;

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

    // 마우스 콜백 함수 등록
    cv::setMouseCallback(windowName, onMouse, nullptr);

    // 프레임 크기 저장
    int frameWidth = camera->Width();
    int frameHeight = camera->Height();
    frameWidthGlobal = frameWidth;  // 전역 변수에 저장 (마우스 콜백에서 사용)

    // ========== 모션 감지 변수 (주석 처리) ==========
    // 이전 프레임의 중심점들을 저장하여 움직임 감지
    // std::vector<cv::Point> previousCenters;
    // const int MOTION_THRESHOLD = 10;  // 10픽셀 이상 움직여야 "움직이는 객체"로 판단

    std::cout << "Instructions:" << std::endl;
    std::cout << "1. Click 4 points on the LEFT image in order: lefttop, righttop, rightbottom, leftbottom" << std::endl;
    std::cout << "2. After selecting 4 points, a warped 1024x768 view will appear in a new window" << std::endl;
    std::cout << "3. Press 'q' to quit" << std::endl;

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
                cv::Mat grayDisplay, binaryDisplay;
                cv::cvtColor(grayFrame, grayDisplay, cv::COLOR_GRAY2BGR);
                cv::cvtColor(binaryFrame, binaryDisplay, cv::COLOR_GRAY2BGR);

                // ===== 중심점 계산 및 표시 =====
                // 현재 프레임에서 검출된 중심점들을 저장
                // std::vector<cv::Point> currentCenters;  // 모션 감지 비활성화로 주석 처리

                // 호모그래피 변환을 위한 중심점 저장 (cv::Point2f 타입)
                std::vector<cv::Point2f> detectedCenters;

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
                        // currentCenters.push_back(center);  // 모션 감지 비활성화로 주석 처리

                        // 호모그래피 변환을 위해 중심점 저장
                        detectedCenters.push_back(cv::Point2f(cx, cy));

                        // ===== 움직임 감지 (주석 처리) =====
                        // 이전 프레임의 중심점들과 비교하여 움직임 판단
                        // 10픽셀 이상 이동했으면 "움직이는 객체"로 판단
                        /*
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
                        */

                        // ===== 모든 객체 표시 (모션 감지 비활성화) =====
                        // 정적/동적 구분 없이 모든 객체의 좌표 표시
                        // if (isMoving)  // 주석 처리: 모든 객체 표시
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

                // ===== 이전 프레임 정보 업데이트 (주석 처리) =====
                // 다음 프레임의 움직임 감지를 위해 현재 중심점들 저장
                // previousCenters = currentCenters;  // 모션 감지 비활성화로 주석 처리

                // ===== 선택된 점 표시 =====
                // 사용자가 클릭한 점들을 grayscale 이미지에 표시
                for (size_t i = 0; i < selectedPoints.size(); i++)
                {
                    // 점 그리기 (파란색 원)
                    cv::circle(grayDisplay, selectedPoints[i], 8, cv::Scalar(255, 0, 0), -1);

                    // 점 번호 표시
                    std::string label = std::to_string(i + 1);
                    cv::putText(grayDisplay, label,
                                cv::Point(selectedPoints[i].x + 10, selectedPoints[i].y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
                }

                // 점들을 선으로 연결
                if (selectedPoints.size() >= 2)
                {
                    for (size_t i = 0; i < selectedPoints.size() - 1; i++)
                    {
                        cv::line(grayDisplay, selectedPoints[i], selectedPoints[i + 1],
                                 cv::Scalar(0, 255, 255), 2);
                    }

                    // 4개 점이 모두 선택되면 사각형 완성
                    if (selectedPoints.size() == 4)
                    {
                        cv::line(grayDisplay, selectedPoints[3], selectedPoints[0],
                                 cv::Scalar(0, 255, 255), 2);
                    }
                }

                // ===== 이미지 합성 =====
                // 왼쪽: Grayscale 원본 영상 (선택된 점 표시)
                // 오른쪽: 이진화 후 움직이는 객체 표시
                cv::Mat combined;
                cv::hconcat(grayDisplay, binaryDisplay, combined);

                // ===== 화면에 표시 =====
                cv::imshow(windowName, combined);

                // ===== 호모그래피 변환 및 표시 =====
                // 4개 점이 모두 선택되면 호모그래피 변환 적용
                if (homographyReady)
                {
                    // 원본 grayscale 이미지에 호모그래피 변환 적용
                    cv::Mat warpedImage;
                    cv::warpPerspective(grayFrame, warpedImage, homographyMatrix,
                                        cv::Size(1024, 768));

                    // Warped 이미지를 컬러로 변환 (점과 텍스트 표시를 위해)
                    cv::Mat warpedColor;
                    cv::cvtColor(warpedImage, warpedColor, cv::COLOR_GRAY2BGR);

                    // 검출된 중심점들을 호모그래피 변환
                    if (!detectedCenters.empty())
                    {
                        std::vector<cv::Point2f> transformedCenters;
                        cv::perspectiveTransform(detectedCenters, transformedCenters, homographyMatrix);

                        // 변환된 좌표를 warped 이미지에 표시
                        for (size_t i = 0; i < transformedCenters.size(); i++)
                        {
                            // 변환된 좌표가 이미지 범위 내에 있는지 확인
                            if (transformedCenters[i].x >= 0 && transformedCenters[i].x < 1024 &&
                                transformedCenters[i].y >= 0 && transformedCenters[i].y < 768)
                            {
                                // 중심점에 빨간색 원 그리기
                                cv::circle(warpedColor, transformedCenters[i], 5, cv::Scalar(0, 0, 255), -1);

                                // 좌표 텍스트를 초록색으로 표시
                                int tx = static_cast<int>(transformedCenters[i].x);
                                int ty = static_cast<int>(transformedCenters[i].y);
                                std::string coordText = "(" + std::to_string(tx) + "," + std::to_string(ty) + ")";
                                cv::putText(warpedColor, coordText,
                                            cv::Point(tx + 10, ty - 5),
                                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
                            }
                        }
                    }

                    // 새 창에 표시 (좌표가 표시된 컬러 이미지)
                    cv::imshow("Warped View (1024x768)", warpedColor);
                }
            }
        }

        // ===== 키 입력 처리 =====
        // 1ms 동안 키 입력 대기 (OpenCV는 이 시간 동안 이벤트 처리)
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) // 'q', 'Q', 또는 ESC 키
        {
            running = false;
        }
        else if (key == 'r' || key == 'R') // 'r' 키: 선택 초기화
        {
            selectedPoints.clear();
            homographyReady = false;
            std::cout << "Point selection reset." << std::endl;
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
