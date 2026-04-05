#pragma once
#include "common/common.h"
#include <string>
#include <memory>
#include <vector>

// Forward declaration — 무거운 ur_rtde 헤더를 .h 에서 숨김
// (.cpp 에서만 include)
namespace ur_rtde {
    class RTDEReceiveInterface;
    class RTDEControlInterface;
}

// ============================================================================
// URHal  — UR 로봇 하드웨어 추상화 계층
//
// 역할:
//   - RTDEReceiveInterface  : 실제 관절각 읽기 (readJointAngles)
//   - RTDEControlInterface  : servoJ 명령 전송 (writeJointAngles)
//   - moveToHome            : moveJ로 홈 위치 이동
//
// config.yaml 에서 읽는 파라미터:
//   ur_ip              : UR 로봇 IP 주소
//   ur:
//     servoj_lookahead_time : servoJ lookahead [s]  (기본 0.05)
//     servoj_gain           : servoJ gain            (기본 700)
//     home_deg              : 홈 위치 [deg × 6]      (기본 [0,-90,0,-90,0,0])
// ============================================================================

class URHal {
public:
    URHal(const YAML::Node& config);
    ~URHal();

    // 연결 / 해제
    bool init();
    void close();

    // 관절각 읽기 — RTDEReceiveInterface::getActualQ()
    bool readJointAngles(URState& state);

    // servoJ 명령 전송 — RTDEControlInterface::servoJ()
    bool writeJointAngles(const URState& state);

    // 홈 이동 — servoJ 중단 후 moveJ
    bool moveToHome();

private:
    std::string ip_;

    // servoJ 파라미터 (레퍼런스: ur_threads.cpp)
    double servoj_dt_;               // 호출 주기 [s] = 1/500Hz = 0.002
    double servoj_lookahead_time_;   // lookahead [s]
    double servoj_gain_;             // gain

    // 홈 위치 [rad]
    double home_q_[6];

    // UR timestamp → 시스템 시간 변환 offset [us]
    int64_t timestamp_offset_us_ = 0;

    // ur_rtde 인터페이스 (unique_ptr: forward declaration 가능 + RAII)
    std::unique_ptr<ur_rtde::RTDEReceiveInterface> rtde_receive_;
    std::unique_ptr<ur_rtde::RTDEControlInterface> rtde_control_;
};
