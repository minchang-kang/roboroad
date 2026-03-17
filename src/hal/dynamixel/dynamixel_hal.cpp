// ============================================================================
// dynamixel_hal.cpp
//
// DynamixelSDK 기반 HAL 구현
//   - init(): 포트 오픈 → Return Delay Time = 0 → 전류 제어 모드 → SyncRead/Write 준비
//   - readAngles(): GroupSyncRead → joint_angle[6] (rad)
//   - writeCurrents(): GroupSyncWrite → goal current [LSB]
// ============================================================================

#include "hal/dynamixel/dynamixel_hal.h"
#include <cstdio>
#include <cmath>

// DXL 엔코더 단위 → rad 변환 (2048 = 0 rad, 4096 ticks = 2π)
static constexpr double POSITION_TO_RAD = 2.0 * M_PI / 4096.0;

// ============================================================================
// 생성자 / 소멸자
// ============================================================================

DynamixelHAL::DynamixelHAL(const YAML::Node& config)
    : port_(config["dynamixel_port"].as<std::string>("/dev/ttyUSB0"))
    , baudrate_(config["dynamixel_baudrate"].as<int>(1000000))
    , port_handler_(nullptr), packet_handler_(nullptr)
    , sync_read_(nullptr), sync_write_(nullptr)
{}

DynamixelHAL::~DynamixelHAL()
{
    close();
}

// ============================================================================
// init — 포트 오픈 → 모터별 설정 → SyncRead/Write 초기화
// ============================================================================

bool DynamixelHAL::init()
{
    port_handler_   = dynamixel::PortHandler::getPortHandler(port_.c_str());
    packet_handler_ = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);

    if (!port_handler_->openPort()) {
        fprintf(stderr, "[DynamixelHAL] 포트 열기 실패: %s\n", port_.c_str());
        return false;
    }
    if (!port_handler_->setBaudRate(baudrate_)) {
        fprintf(stderr, "[DynamixelHAL] 보드레이트 설정 실패: %d\n", baudrate_);
        return false;
    }
    printf("[DynamixelHAL] 포트 오픈: %s @ %d bps\n", port_.c_str(), baudrate_);

    // ── 모터별 초기화 ───────────────────────────────────────────────────
    for (int i = 0; i < DOF; i++) {
        uint8_t id = MOTOR_IDS[i];

        // 1. 토크 OFF (설정 변경 전 필수)
        if (!writeReg1(id, ADDR_TORQUE_EN, 0)) {
            fprintf(stderr, "[DynamixelHAL] 토크 OFF 실패 ID %d\n", id);
            return false;
        }
        // 2. Return Delay Time = 0 (응답 지연 제거)
        if (!writeReg1(id, ADDR_RETURN_DELAY, 0)) {
            fprintf(stderr, "[DynamixelHAL] Return Delay Time 설정 실패 ID %d\n", id);
            return false;
        }
        // 3. 전류 제어 모드
        if (!writeReg1(id, ADDR_OP_MODE, MODE_CURRENT_CTRL)) {
            fprintf(stderr, "[DynamixelHAL] 전류 제어 모드 설정 실패 ID %d\n", id);
            return false;
        }
        // 4. 토크 ON
        if (!writeReg1(id, ADDR_TORQUE_EN, 1)) {
            fprintf(stderr, "[DynamixelHAL] 토크 ON 실패 ID %d\n", id);
            return false;
        }
        printf("[DynamixelHAL]   ID %d 초기화 완료 (ReturnDelay=0, CurrentCtrl)\n", id);
    }

    // ── GroupSyncRead 초기화 (PRESENT_POSITION, 4byte) ──────────────────
    sync_read_ = new dynamixel::GroupSyncRead(
        port_handler_, packet_handler_, ADDR_PRESENT_POS, LEN_PRESENT_POS);
    for (int i = 0; i < DOF; i++) {
        if (!sync_read_->addParam(MOTOR_IDS[i])) {
            fprintf(stderr, "[DynamixelHAL] SyncRead addParam 실패 ID %d\n", MOTOR_IDS[i]);
            return false;
        }
    }

    // ── GroupSyncWrite 초기화 (GOAL_CURRENT, 2byte) ─────────────────────
    sync_write_ = new dynamixel::GroupSyncWrite(
        port_handler_, packet_handler_, ADDR_GOAL_CUR, LEN_GOAL_CUR);

    printf("[DynamixelHAL] 초기화 완료\n");
    return true;
}

// ============================================================================
// close
// ============================================================================

void DynamixelHAL::close()
{
    // 전류 0으로 클리어 후 토크 OFF
    if (packet_handler_ && port_handler_) {
        int16_t zero[DOF] = {};
        if (sync_write_)
            writeCurrents(zero);
        for (int i = 0; i < DOF; i++)
            writeReg1(MOTOR_IDS[i], ADDR_TORQUE_EN, 0);
    }

    delete sync_write_;  sync_write_ = nullptr;
    delete sync_read_;   sync_read_  = nullptr;

    if (port_handler_) {
        port_handler_->closePort();
        port_handler_ = nullptr;
    }
    printf("[DynamixelHAL] 종료\n");
}

// ============================================================================
// readAngles — GroupSyncRead → joint_angle[6] (rad)
// ============================================================================

bool DynamixelHAL::readAngles(MasterState& state)
{
    if (sync_read_->txRxPacket() != COMM_SUCCESS)
        return false;

    for (int i = 0; i < DOF; i++) {
        uint8_t id = MOTOR_IDS[i];
        if (!sync_read_->isAvailable(id, ADDR_PRESENT_POS, LEN_PRESENT_POS))
            return false;

        int32_t raw = static_cast<int32_t>(
            sync_read_->getData(id, ADDR_PRESENT_POS, LEN_PRESENT_POS));

        // 2048 = 중립(0 rad)
        state.joint_angle[i] = (raw - 2048) * POSITION_TO_RAD;
    }
    return true;
}

// ============================================================================
// writeCurrents — GroupSyncWrite → goal current [LSB]
// ============================================================================

bool DynamixelHAL::writeCurrents(const int16_t* goal_cur)
{
    if (!sync_write_)
        return false;

    sync_write_->clearParam();
    for (int i = 0; i < DOF; i++) {
        uint8_t data[2] = {
            static_cast<uint8_t>( goal_cur[i]       & 0xFF),
            static_cast<uint8_t>((goal_cur[i] >> 8) & 0xFF)
        };
        if (!sync_write_->addParam(MOTOR_IDS[i], data))
            return false;
    }
    return sync_write_->txPacket() == COMM_SUCCESS;
}

// ============================================================================
// writeTorque — MasterState.torque (Nm) 기반 전류 명령 (미사용, 예비)
// ============================================================================

bool DynamixelHAL::writeTorque(const MasterState& state) { return true; }

// ============================================================================
// setTorqueEnable / setOperatingMode
// ============================================================================

bool DynamixelHAL::setTorqueEnable(bool enable)
{
    uint8_t val = enable ? 1 : 0;
    for (int i = 0; i < DOF; i++) {
        if (!writeReg1(MOTOR_IDS[i], ADDR_TORQUE_EN, val))
            return false;
    }
    return true;
}

bool DynamixelHAL::setOperatingMode(int mode)
{
    for (int i = 0; i < DOF; i++) {
        writeReg1(MOTOR_IDS[i], ADDR_TORQUE_EN, 0);
        if (!writeReg1(MOTOR_IDS[i], ADDR_OP_MODE, static_cast<uint8_t>(mode)))
            return false;
        writeReg1(MOTOR_IDS[i], ADDR_TORQUE_EN, 1);
    }
    return true;
}

// ============================================================================
// writeReg1 — 1바이트 레지스터 쓰기 헬퍼
// ============================================================================

bool DynamixelHAL::writeReg1(uint8_t id, uint16_t addr, uint8_t val)
{
    uint8_t err = 0;
    return packet_handler_->write1ByteTxRx(port_handler_, id, addr, val, &err) == COMM_SUCCESS;
}
