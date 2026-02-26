#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "homography.h"
#include <iostream>
#include <algorithm>

void onMouse(int event, int x, int y, int flags, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;

    MouseCallbackData* md = static_cast<MouseCallbackData*>(userdata);
    HomographyState*   hs = md->state;

    HWND hwnd = FindWindowA(NULL, md->windowName.c_str());
    if (!hwnd) return;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right;
    int clientH = rc.bottom;
    if (clientW <= 0 || clientH <= 0) return;

    int imgW = md->frameWidth * 2;
    int imgH = md->frameHeight;

    // WINDOW_AUTOSIZE 기준: scale=1, offset=0 이지만 범용 letterbox 계산 유지
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

    // 좌측 이미지(원본)에서만 점 선택
    if (imgX < md->frameWidth &&
        static_cast<int>(hs->selectedPoints.size()) < HomographyState::REQUIRED_POINTS)
    {
        hs->selectedPoints.push_back(cv::Point2f(imgX, imgY));
        std::cout << "Point " << hs->selectedPoints.size() << " selected (actual): ("
                  << static_cast<int>(imgX) << ", " << static_cast<int>(imgY) << ")" << std::endl;

        if (static_cast<int>(hs->selectedPoints.size()) == HomographyState::REQUIRED_POINTS)
        {
            std::cout << "All 4 points selected. Calculating homography..." << std::endl;

            float tw = static_cast<float>(md->targetWidth  - 1);
            float th = static_cast<float>(md->targetHeight - 1);
            std::vector<cv::Point2f> dstPoints = {
                {0.f, 0.f}, {tw, 0.f}, {tw, th}, {0.f, th}
            };
            hs->matrix = cv::getPerspectiveTransform(hs->selectedPoints, dstPoints);
            hs->ready  = true;

            std::cout << "Homography matrix calculated. Warped view ready." << std::endl;
        }
    }
}
