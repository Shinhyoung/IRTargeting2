#include "homography.h"
#include <iostream>

void onMouse(int event, int x, int y, int flags, void* userdata)
{
    if (event != cv::EVENT_LBUTTONDOWN) return;

    MouseCallbackData* md = static_cast<MouseCallbackData*>(userdata);
    HomographyState*   hs = md->state;

    int imgW = md->frameWidth * 2;
    int imgH = md->frameHeight;

    // WINDOW_AUTOSIZE: OpenCV mouse callback은 이미 이미지 좌표를 반환함 (1:1).
    // GetClientRect + letterbox 변환은 WINDOW_NORMAL(리사이징 가능 창)용이며,
    // WINDOW_AUTOSIZE에서 사용하면 저해상도 화면/DPI 스케일링 환경에서
    // GetClientRect가 클리핑된 크기를 반환해 좌표가 잘못 변환되는 버그가 발생.
    if (x < 0 || y < 0 || x >= imgW || y >= imgH) return;

    float imgX = static_cast<float>(x);
    float imgY = static_cast<float>(y);

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
