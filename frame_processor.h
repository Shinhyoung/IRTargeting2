#pragma once

#include <opencv2/opencv.hpp>
#include "homography.h"
#include "settings.h"

// ========== 프레임 처리 결과 ==========
struct FrameResult
{
    cv::Mat leftPanel;                          // Grayscale 원본 + 선택점 표시
    cv::Mat rightPanel;                         // Binary (호모그래피 전) 또는 Warped 컬러 (후)
    std::vector<cv::Point2f> detectedCenters;   // 원본에서 검출된 모든 중심점
    std::vector<cv::Point2f> inBoundCenters;    // 호모그래피 영역 내 중심점 (UDP 전송 대상)
};

// raw 프레임 데이터를 받아 처리 결과를 반환
FrameResult processFrame(
    const unsigned char* rawData,
    int                  width,
    int                  height,
    const HomographyState& hom,
    const AppSettings&     settings);
