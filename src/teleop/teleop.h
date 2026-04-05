#pragma once
#include "common/common.h"
#include "hal/ur/ur_hal.h"

// ============================================================================
// Teleop
//
// 마스터(Dynamixel) 관절각 → 슬레이브(UR) servoJ 명령 변환
//
// 변환 과정:
//   1. TELEOP 플래그 확인 — OFF 이면 즉시 반환
//   2. 축 방향 반전 (joint_direction) 및 UR 홈 오프셋 적용
//   3. EMA 저역통과 필터로 노이즈 제거
//   4. URHal::writeJointAngles() 호출
//
// config.yaml 에서 읽는 파라미터 (teleop: 섹션):
//   joint_direction  : [±1, ±1, ...] — DXL CCW = URDF + 방향이면 +1, 아니면 -1
//   home_offset_deg  : [deg, deg, ...] — DXL 2048(중립) 이 대응하는 UR 관절각
//   ema_alpha        : 0~1 — EMA 필터 계수 (기본 0.4)
// ============================================================================

class Teleop {
public:
    Teleop(URHal& ur, const YAML::Node& config);
    ~Teleop();

    // 500Hz 루프에서 호출 — master 상태를 UR 명령으로 변환하여 전송
    void update(const MasterState& master, bool teleop_on);

    // EMA 필터 초기화 (재시작 시)
    void reset();

private:
    URHal& ur_;

    // EMA 저역통과 필터
    double ema_alpha_;       // 필터 계수 (0~1), 클수록 응답 빠름
    double ema_q_[6];        // 필터 상태 (이전 출력값)
    bool   ema_init_;        // 첫 샘플 초기화 여부

    // 축 매핑 파라미터
    double joint_direction_[6];   // +1 or -1
    double home_offset_rad_[6];   // UR 홈 관절각 [rad]
};
