#pragma once

#include <opencv2/opencv.hpp>

// ========== OSD 렌더링에 필요한 상태 ==========
struct OSDState
{
    bool continuousSend;
    bool homographyReady;
    int  selectedPointCount;
    int  displayCount;   // 하단 OSD 표시 수: !homographyReady → detectedCenters.size(),
                         //                     homographyReady  → inBoundCenters.size()
};

void renderOSD(cv::Mat& image, const OSDState& state);
