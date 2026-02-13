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

int main(int argc, char* argv[])
{
    // Open log file for debugging
    std::ofstream logFile("IRViewer_log.txt");
    auto cout_buf = std::cout.rdbuf(logFile.rdbuf());
    auto cerr_buf = std::cerr.rdbuf(logFile.rdbuf());

    std::cout << "=== OptiTrack Flex 13 Camera IR Viewer (Camera SDK) ===" << std::endl;
    std::cout << "Log started at: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;

    // Initialize Camera SDK
    std::cout << "Initializing Camera SDK..." << std::endl;
    CameraManager::X().WaitForInitialization();

    if (!CameraManager::X().AreCamerasInitialized())
    {
        std::cerr << "Failed to initialize cameras." << std::endl;
        std::cerr << "Please check:" << std::endl;
        std::cerr << "1. Camera is connected via USB" << std::endl;
        std::cerr << "2. Camera drivers are installed" << std::endl;
        std::cerr << "3. No other software (Motive) is using the camera" << std::endl;

        std::cout.rdbuf(cout_buf);
        std::cerr.rdbuf(cerr_buf);
        logFile.close();

        MessageBoxA(NULL, "Failed to initialize Camera SDK. Check IRViewer_log.txt for details.", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "Camera SDK initialized successfully." << std::endl;

    // Get camera count
    CameraList list;
    std::cout << "Number of cameras detected: " << list.Count() << std::endl;

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

    // Log camera information from list
    for (int i = 0; i < list.Count(); i++)
    {
        std::cout << "Camera " << i << ": " << list[i].Name() << std::endl;
        std::cout << "  UID: " << list[i].UID() << std::endl;
        std::cout << "  Initial State: " << list[i].State() << std::endl;
    }

    // Wait for camera to reach Initialized state (state 6)
    std::cout << "Waiting for camera to fully initialize..." << std::endl;
    const int maxWaitSeconds = 10;
    bool cameraReady = false;

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

    // Use the first camera from the list
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

    std::cout << "Camera Serial: " << camera->Serial() << std::endl;
    std::cout << "Camera Name: " << camera->Name() << std::endl;
    std::cout << "Camera Resolution: " << camera->Width() << "x" << camera->Height() << std::endl;

    // Set video mode to Grayscale to get raw IR image
    std::cout << "Setting camera to Grayscale mode..." << std::endl;
    camera->SetVideoType(Core::GrayscaleMode);
    std::cout << "Camera set to Grayscale mode." << std::endl;

    // Turn off IR illumination by setting intensity to 0
    std::cout << "Disabling IR illumination..." << std::endl;
    camera->SetIntensity(0);
    std::cout << "IR illumination disabled (intensity set to 0)." << std::endl;

    // Start camera
    std::cout << "Starting camera..." << std::endl;
    camera->Start();
    std::cout << "Camera started. Press 'q' to quit." << std::endl;

    // Create OpenCV window
    std::string windowName = "OptiTrack Flex 13 - IR View";
    cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);

    int frameWidth = camera->Width();
    int frameHeight = camera->Height();

    // Variables for motion detection
    std::vector<cv::Point> previousCenters;
    const int MOTION_THRESHOLD = 10;

    bool running = true;
    while (running)
    {
        // Get latest frame from camera
        std::shared_ptr<const Frame> frame = camera->LatestFrame();

        if (frame && frame->IsGrayscale())
        {
            // Get grayscale data
            const unsigned char* grayscaleData = frame->GrayscaleData(*camera);

            if (grayscaleData)
            {
                // Create OpenCV Mat from grayscale data
                cv::Mat grayFrame(frameHeight, frameWidth, CV_8UC1, (void*)grayscaleData);

                // Apply morphological dilation 3 times
                cv::Mat dilatedFrame;
                cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
                cv::dilate(grayFrame, dilatedFrame, kernel, cv::Point(-1, -1), 3);

                // Apply binary threshold (200)
                cv::Mat binaryFrame;
                cv::threshold(dilatedFrame, binaryFrame, 200, 255, cv::THRESH_BINARY);

                // Find contours in binary image
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(binaryFrame.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                // Create color images for display
                cv::Mat dilatedDisplay, binaryDisplay;
                cv::cvtColor(dilatedFrame, dilatedDisplay, cv::COLOR_GRAY2BGR);
                cv::cvtColor(binaryFrame, binaryDisplay, cv::COLOR_GRAY2BGR);

                // Calculate and draw center points on binary image
                std::vector<cv::Point> currentCenters;

                for (const auto& contour : contours)
                {
                    // Calculate moments
                    cv::Moments m = cv::moments(contour);

                    if (m.m00 > 0) // Avoid division by zero
                    {
                        // Calculate center coordinates
                        int cx = static_cast<int>(m.m10 / m.m00);
                        int cy = static_cast<int>(m.m01 / m.m00);
                        cv::Point center(cx, cy);
                        currentCenters.push_back(center);

                        // Check if this point is moving (distance >= 10 pixels from any previous point)
                        bool isMoving = true;
                        for (const auto& prevCenter : previousCenters)
                        {
                            double distance = cv::norm(center - prevCenter);
                            if (distance < MOTION_THRESHOLD)
                            {
                                isMoving = false;
                                break;
                            }
                        }

                        // Only draw center point and coordinates if the point is moving
                        if (isMoving)
                        {
                            // Draw center point (red circle)
                            cv::circle(binaryDisplay, center, 5, cv::Scalar(0, 0, 255), -1);

                            // Draw coordinates text (green)
                            std::string coordText = "(" + std::to_string(cx) + "," + std::to_string(cy) + ")";
                            cv::putText(binaryDisplay, coordText, cv::Point(cx + 10, cy - 5),
                                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
                        }
                    }
                }

                // Update previous centers for next frame
                previousCenters = currentCenters;

                // Combine before and after images side by side
                cv::Mat combined;
                cv::hconcat(dilatedDisplay, binaryDisplay, combined);

                // Display combined frame (left: before binarization, right: after binarization with centers)
                cv::imshow(windowName, combined);
            }
        }

        // Check for key press (wait 1ms)
        int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) // 'q', 'Q', or ESC
        {
            running = false;
        }

        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    cv::destroyAllWindows();
    CameraManager::X().Shutdown();

    std::cout << "Program terminated successfully." << std::endl;

    // Restore standard streams
    std::cout.rdbuf(cout_buf);
    std::cerr.rdbuf(cerr_buf);
    logFile.close();

    return 0;
}
