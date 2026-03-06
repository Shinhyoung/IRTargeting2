#include "udp_sender.h"
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <chrono>

UDPSender::~UDPSender()
{
    stopThread();
    if (socket_ != INVALID_SOCKET)
        closesocket(socket_);
}

bool UDPSender::init(const std::string& ip, int port)
{
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET)
    {
        std::cerr << "UDP socket creation failed. Error: " << WSAGetLastError() << std::endl;
        return false;
    }
    updateTarget(ip, port);
    return true;
}

void UDPSender::updateTarget(const std::string& ip, int port)
{
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port   = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    std::cout << "UDP target: " << ip << ":" << port << std::endl;
}

void UDPSender::startThread(int fps)
{
    if (threadRunning_.load()) return;
    fps_.store(fps);
    threadRunning_.store(true);
    sendThread_ = std::thread(&UDPSender::sendLoop, this);
    std::cout << "[UDP] Send thread started at " << fps << " FPS." << std::endl;
}

void UDPSender::stopThread()
{
    if (!threadRunning_.load()) return;
    threadRunning_.store(false);
    if (sendThread_.joinable())
        sendThread_.join();
    std::cout << "[UDP] Send thread stopped." << std::endl;
}

void UDPSender::setFps(int fps)
{
    fps_.store(fps);
    std::cout << "[UDP] FPS updated to " << fps << std::endl;
}

void UDPSender::updatePoints(const std::vector<cv::Point2f>& points)
{
    std::lock_guard<std::mutex> lock(mutex_);
    points_ = points;
}

void UDPSender::sendLoop()
{
    int sendCount = 0;
    auto secStart = std::chrono::steady_clock::now();

    while (threadRunning_.load())
    {
        auto start = std::chrono::steady_clock::now();

        // 최신 좌표 복사
        std::vector<cv::Point2f> pts;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pts = points_;
        }

        if (!pts.empty())
        {
            sendPacket(pts);
            ++sendCount;
        }

        // 1초마다 실제 FPS 갱신
        auto now = std::chrono::steady_clock::now();
        auto secElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - secStart).count();
        if (secElapsed >= 1000)
        {
            actualFps_.store(sendCount);
            sendCount = 0;
            secStart = now;
        }

        // 목표 간격만큼 대기
        int targetUs = 1000000 / fps_.load();
        auto elapsed = now - start;
        auto sleepTime = std::chrono::microseconds(targetUs) -
                         std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
        if (sleepTime.count() > 0)
            std::this_thread::sleep_for(sleepTime);
    }
    actualFps_.store(0);
}

void UDPSender::sendPacket(const std::vector<cv::Point2f>& points)
{
    if (socket_ == INVALID_SOCKET || points.empty()) return;

    std::string msg;
    for (size_t i = 0; i < points.size(); i++)
    {
        if (i > 0) msg += ";";
        msg += std::to_string(static_cast<int>(points[i].x))
             + ","
             + std::to_string(static_cast<int>(points[i].y));
    }
    sendto(socket_, msg.c_str(), static_cast<int>(msg.length()),
           0, reinterpret_cast<const sockaddr*>(&addr_), sizeof(addr_));
}
