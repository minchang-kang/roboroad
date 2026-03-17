#pragma once
#include "common/common.h"
#include <string>
#include <dynamixel_sdk.h>

// ─── 제어 테이블 주소 (XM430 / XM540 공통) ──────────────────
static constexpr uint16_t ADDR_RETURN_DELAY = 9;
static constexpr uint16_t ADDR_OP_MODE      = 11;
static constexpr uint16_t ADDR_TORQUE_EN    = 64;
static constexpr uint16_t ADDR_GOAL_CUR     = 102;
static constexpr uint16_t ADDR_PRESENT_POS  = 132;
static constexpr uint16_t LEN_PRESENT_POS   = 4;   // int32
static constexpr uint16_t LEN_GOAL_CUR      = 2;   // int16
static constexpr uint8_t  MODE_CURRENT_CTRL = 0;
static constexpr uint8_t  PROTOCOL_VERSION  = 2;
static constexpr int      DOF               = 6;
static constexpr uint8_t  MOTOR_IDS[DOF]    = {1, 2, 3, 4, 5, 6};

class DynamixelHAL {
public:
    DynamixelHAL(const YAML::Node& config);
    ~DynamixelHAL();

    bool init();
    void close();

    bool readAngles(MasterState& state);       // GroupSyncRead → joint_angle[6] (rad)
    bool writeTorque(const MasterState& state);
    bool writeCurrents(const int16_t* goal_cur); // GroupSyncWrite → goal current [LSB]
    bool setTorqueEnable(bool enable);
    bool setOperatingMode(int mode);

private:
    std::string port_;
    int         baudrate_;

    dynamixel::PortHandler*    port_handler_   = nullptr;
    dynamixel::PacketHandler*  packet_handler_ = nullptr;
    dynamixel::GroupSyncRead*  sync_read_      = nullptr;
    dynamixel::GroupSyncWrite* sync_write_     = nullptr;

    bool writeReg1(uint8_t id, uint16_t addr, uint8_t val);
};