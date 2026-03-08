#pragma once
#include "common/common.h"
#include <string>

static constexpr int DOF = 6;

class DynamixelHAL {
public:
    DynamixelHAL(const YAML::Node& config);
    ~DynamixelHAL();

    bool init();
    void close();

    bool readAngles(MasterState& state);
    bool writeTorque(const MasterState& state);
    bool setTorqueEnable(bool enable);
    bool setOperatingMode(int mode);
    // 토크 off 함수, 포지션 제어 변경 함수, 토크 제어 변경 함수,

private:
    std::string port_;
    int baudrate_;

    // DynamixelSDK 관련 객체들
    // PacketHandler, PortHandler, GroupSyncRead, GroupSyncWrite
};