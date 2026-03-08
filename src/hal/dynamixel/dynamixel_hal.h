#pragma once
#include "common/common.h"
#include <string>

static constexpr int DOF = 6;

class DynamixelHAL {
public:
    DynamixelHAL(const std::string& port, int baudrate);
    ~DynamixelHAL();

    bool init();
    void close();

    bool readAngles(MasterState& state);
    bool writeTorque(const MasterState& state);
    bool setTorqueEnable(bool enable);

private:
    std::string port_;
    int baudrate_;

    // DynamixelSDK 관련 객체들
    // PacketHandler, PortHandler, GroupSyncRead, GroupSyncWrite
};