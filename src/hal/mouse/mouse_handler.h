#pragma once

#include <string>
#include <atomic>
#include <yaml-cpp/yaml.h>
#include <dynamixel_sdk.h>
#include "../../common/common.h"

// MouseHandler — 마우스 좌클릭으로 다이나믹셀 모터 단일 제어
// - 6축 GC 로봇(DynamixelHAL)과 완전히 독립된 별도 포트 사용
// - 좌클릭 press → SPRAY 플래그 ON + 모터 전류 명령
// - 좌클릭 release → SPRAY 플래그 OFF + 모터 전류 0
class MouseHandler {
public:
    explicit MouseHandler(const YAML::Node& config);
    ~MouseHandler() = default;

    bool init();
    void run(SharedContext& ctx, const std::atomic<bool>& running);
    void close();

    static std::string findDevicePath(const std::string& device_name);

private:
    // 마우스 장치
    std::string device_name_;
    int         mouse_fd_ = -1;

    // 다이나믹셀 (모터 10 전용 포트)
    std::string port_;
    int         baudrate_;
    uint8_t     motor_id_;
    int16_t     goal_current_;

    dynamixel::PortHandler*   port_handler_   = nullptr;
    dynamixel::PacketHandler* packet_handler_ = nullptr;

    bool writeCurrent(int16_t current);
};
