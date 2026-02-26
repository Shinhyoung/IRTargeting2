#include "frame_processor.h"
#include <string>

// ─────────────────────────────────────────────────────────
//  내부 헬퍼 함수 (파일 static)
// ─────────────────────────────────────────────────────────

// Dilate → Threshold → Contour 검출 → 중심점 계산
// binaryDisplayOut: 시각화용 컬러 바이너리 이미지 (BGR)
static std::vector<cv::Point2f> detectCenters(
    const cv::Mat& gray,
    cv::Mat&       binaryDisplayOut)
{
    cv::Mat dilated;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::dilate(gray, dilated, kernel, cv::Point(-1, -1), 3);

    cv::Mat binary;
    cv::threshold(dilated, binary, 200, 255, cv::THRESH_BINARY);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    cv::cvtColor(binary, binaryDisplayOut, cv::COLOR_GRAY2BGR);

    std::vector<cv::Point2f> centers;
    for (const auto& contour : contours)
    {
        cv::Moments m = cv::moments(contour);
        if (m.m00 > 0)
        {
            int cx = static_cast<int>(m.m10 / m.m00);
            int cy = static_cast<int>(m.m01 / m.m00);
            centers.emplace_back(static_cast<float>(cx), static_cast<float>(cy));

            cv::circle(binaryDisplayOut, cv::Point(cx, cy), 5, cv::Scalar(0, 0, 255), -1);
            std::string txt = "(" + std::to_string(cx) + "," + std::to_string(cy) + ")";
            cv::putText(binaryDisplayOut, txt, cv::Point(cx + 10, cy - 5),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
        }
    }
    return centers;
}

// 선택된 4점을 왼쪽 패널에 표시
static void drawSelectedPoints(cv::Mat& img, const std::vector<cv::Point2f>& pts)
{
    for (size_t i = 0; i < pts.size(); i++)
    {
        cv::circle(img, pts[i], 8, cv::Scalar(255, 0, 0), -1);
        cv::putText(img, std::to_string(i + 1),
                    cv::Point(static_cast<int>(pts[i].x) + 10,
                              static_cast<int>(pts[i].y) - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
    }
    if (pts.size() >= 2)
    {
        for (size_t i = 0; i < pts.size() - 1; i++)
            cv::line(img, pts[i], pts[i + 1], cv::Scalar(0, 255, 255), 2);
        if (pts.size() == 4)
            cv::line(img, pts[3], pts[0], cv::Scalar(0, 255, 255), 2);
    }
}

// 호모그래피 적용 패널 생성 + 경계 내 좌표 수집
static cv::Mat buildWarpedPanel(
    const cv::Mat&                  gray,
    const std::vector<cv::Point2f>& detectedCenters,
    const HomographyState&          hom,
    const AppSettings&              settings,
    std::vector<cv::Point2f>&       inBoundOut)
{
    cv::Mat warped;
    cv::warpPerspective(gray, warped, hom.matrix,
                        cv::Size(settings.targetWidth, settings.targetHeight));
    cv::Mat warpedColor;
    cv::cvtColor(warped, warpedColor, cv::COLOR_GRAY2BGR);

    if (!detectedCenters.empty())
    {
        std::vector<cv::Point2f> transformed;
        cv::perspectiveTransform(detectedCenters, transformed, hom.matrix);

        for (size_t i = 0; i < transformed.size(); i++)
        {
            if (transformed[i].x >= 0 && transformed[i].x < settings.targetWidth &&
                transformed[i].y >= 0 && transformed[i].y < settings.targetHeight)
            {
                inBoundOut.push_back(transformed[i]);

                cv::circle(warpedColor, transformed[i], 5, cv::Scalar(0, 0, 255), -1);
                int tx = static_cast<int>(transformed[i].x);
                int ty = static_cast<int>(transformed[i].y);
                std::string txt = "(" + std::to_string(tx) + "," + std::to_string(ty) + ")";
                cv::putText(warpedColor, txt, cv::Point(tx + 10, ty - 5),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 0), 1);
            }
        }
    }
    return warpedColor;
}

// ─────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────

FrameResult processFrame(
    const unsigned char* rawData,
    int                  width,
    int                  height,
    const HomographyState& hom,
    const AppSettings&     settings)
{
    FrameResult result;

    cv::Mat grayFrame(height, width, CV_8UC1, const_cast<unsigned char*>(rawData));

    // 왼쪽 패널: Grayscale + 선택점
    cv::cvtColor(grayFrame, result.leftPanel, cv::COLOR_GRAY2BGR);
    drawSelectedPoints(result.leftPanel, hom.selectedPoints);

    // 중심점 검출
    cv::Mat binaryDisplay;
    result.detectedCenters = detectCenters(grayFrame, binaryDisplay);

    // 오른쪽 패널: 호모그래피 전 → Binary, 후 → Warped
    if (hom.ready)
    {
        cv::Mat warpedPanel = buildWarpedPanel(
            grayFrame, result.detectedCenters, hom, settings, result.inBoundCenters);
        cv::resize(warpedPanel, result.rightPanel, cv::Size(width, height));
    }
    else
    {
        result.rightPanel = binaryDisplay;
    }

    return result;
}
