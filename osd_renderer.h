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
    bool configSaved;    // true 이면 화면 중앙에 "Config Saved!" 2초간 표시
};

void renderOSD(cv::Mat& image, const OSDState& state);
