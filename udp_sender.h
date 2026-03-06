#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

#include <opencv2/core/types.hpp>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

// ========== UDP 전송 클래스 (별도 스레드) ==========
class UDPSender
{
public:
    UDPSender() = default;
    ~UDPSender();

    // WSAStartup은 호출자(main)가 담당. 소켓만 생성/관리.
    bool init(const std::string& ip, int port);

    // P키로 설정 변경 시 대상 주소 갱신
    void updateTarget(const std::string& ip, int port);

    // 전송 스레드 시작/중지
    void startThread(int fps);
    void stopThread();

    // 전송 FPS 런타임 변경
    void setFps(int fps);

    // 메인 루프에서 호출: 최신 좌표를 스레드에 전달
    void updatePoints(const std::vector<cv::Point2f>& points);

    bool isRunning() const { return threadRunning_.load(); }
    int  actualFps()  const { return actualFps_.load(); }

private:
    SOCKET      socket_ = INVALID_SOCKET;
    sockaddr_in addr_   = {};

    // 전송 스레드
    std::thread         sendThread_;
    std::mutex          mutex_;
    std::atomic<bool>   threadRunning_{false};
    std::atomic<int>    fps_{60};
    std::atomic<int>    actualFps_{0};
    std::vector<cv::Point2f> points_;   // mutex_ 로 보호

    void sendLoop();
    void sendPacket(const std::vector<cv::Point2f>& points);
};
