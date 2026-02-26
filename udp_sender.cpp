#include "udp_sender.h"
#include <ws2tcpip.h>
#include <iostream>
#include <string>

UDPSender::~UDPSender()
{
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

void UDPSender::send(const std::vector<cv::Point2f>& points)
{
    if (socket_ == INVALID_SOCKET || points.empty()) return;

    // 패킷 포맷: "x1,y1;x2,y2;..."
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
