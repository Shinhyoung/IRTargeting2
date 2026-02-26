#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

// ========== 호모그래피 상태 ==========
struct HomographyState
{
    static constexpr int REQUIRED_POINTS = 4;

    std::vector<cv::Point2f> selectedPoints;
    cv::Mat                  matrix;
    bool                     ready = false;

    void reset()
    {
        selectedPoints.clear();
        ready = false;
    }
};

// ========== 마우스 콜백용 데이터 구조체 ==========
struct MouseCallbackData
{
    std::string      windowName;
    int              frameWidth;
    int              frameHeight;
    int              targetWidth;
    int              targetHeight;
    HomographyState* state;    // 전역 대신 포인터로 주입
};

void onMouse(int event, int x, int y, int flags, void* userdata);
