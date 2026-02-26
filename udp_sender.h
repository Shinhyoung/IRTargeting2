#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

#include <opencv2/core/types.hpp>
#include <vector>
#include <string>

// ========== UDP 전송 클래스 ==========
class UDPSender
{
public:
    UDPSender() = default;
    ~UDPSender();

    // WSAStartup은 호출자(main)가 담당. 소켓만 생성/관리.
    bool init(const std::string& ip, int port);

    // P키로 설정 변경 시 대상 주소 갱신
    void updateTarget(const std::string& ip, int port);

    // 패킷 포맷: "x1,y1;x2,y2;..."
    void send(const std::vector<cv::Point2f>& points);

private:
    SOCKET      socket_ = INVALID_SOCKET;
    sockaddr_in addr_   = {};
};
