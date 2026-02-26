#include "osd_renderer.h"
#include <string>

// 그림자 효과로 가독성 있는 텍스트 출력 (파일 static)
static void putKey(cv::Mat& img, const std::string& t, cv::Scalar color, int y)
{
    cv::putText(img, t, cv::Point(10, y),
                cv::FONT_HERSHEY_SIMPLEX, 0.42,
                cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
    cv::putText(img, t, cv::Point(10, y),
                cv::FONT_HERSHEY_SIMPLEX, 0.42,
                color, 1, cv::LINE_AA);
}

void renderOSD(cv::Mat& image, const OSDState& state)
{
    // ===== 상단 OSD: 단축키 안내 =====
    {
        bool showProgress = (state.selectedPointCount > 0 && !state.homographyReady);
        int  boxH         = showProgress ? 120 : 102;

        cv::Mat overlay = image.clone();
        cv::rectangle(overlay, cv::Point(4, 4), cv::Point(252, boxH),
                      cv::Scalar(15, 15, 15), cv::FILLED);
        cv::addWeighted(overlay, 0.65, image, 0.35, 0, image);

        std::string sendLabel = std::string("[S] UDP: ") +
                                (state.continuousSend ? "ON  (Sending)" : "OFF");
        cv::Scalar sendColor  = state.continuousSend
                               ? cv::Scalar(60, 255, 60)
                               : cv::Scalar(120, 200, 255);

        putKey(image, "[Q/ESC] Quit",                   cv::Scalar(200,200,200), 22);
        putKey(image, sendLabel,                         sendColor,               40);
        putKey(image, "[R] Reset Corner Points",         cv::Scalar(200,200,200), 58);
        putKey(image, "[P] Settings",                    cv::Scalar(200,200,200), 76);
        putKey(image, "[L-Click] Select Corner (4pts)",  cv::Scalar(200,200,200), 94);

        if (showProgress)
            putKey(image,
                   "  -> " + std::to_string(state.selectedPointCount) + "/4 pts selected",
                   cv::Scalar(255, 190, 60), 112);
    }

    // ===== 하단 OSD: 전송 상태 및 검출 포인트 수 =====
    {
        std::string statusStr  = state.continuousSend ? "● UDP SENDING" : "○ UDP STOPPED";
        cv::Scalar  statusColor = state.continuousSend
                                 ? cv::Scalar(60, 255, 60)
                                 : cv::Scalar(120, 120, 120);
        cv::putText(image, statusStr,
                    cv::Point(8, image.rows - 8),
                    cv::FONT_HERSHEY_SIMPLEX, 0.50, statusColor, 1, cv::LINE_AA);

        if (state.displayCount > 0)
        {
            std::string ptStr = "Detected: " + std::to_string(state.displayCount) + " pt(s)";
            cv::putText(image, ptStr,
                        cv::Point(image.cols - 190, image.rows - 8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.50, cv::Scalar(0, 220, 220), 1, cv::LINE_AA);
        }
    }
}
