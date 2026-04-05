// ============================================================================
// teleop.cpp
//
// [Teleop::update]  500Hz 루프에서 호출
//   마스터(Dynamixel) 관절각 → 축 방향 변환 → EMA 필터 → UR servoJ 명령
// ============================================================================

#include "teleop/teleop.h"
#include <cmath>
#include <algorithm>
#include <iostream>

// ============================================================================
// 생성자
// ============================================================================

Teleop::Teleop(URHal& ur, const YAML::Node& config)
    : ur_(ur)
    , ema_alpha_(0.4)
    , ema_init_(false)
{
    // ── 기본값 (레퍼런스 config.h 기준) ────────────────────────────────────
    // JOINT_DIRECTION : DXL CCW = URDF + 방향이면 +1, 반대면 -1
    const double default_dir[6]    = { 1.0, -1.0, -1.0,  1.0,  1.0,  1.0 };
    // UR_HOME_OFFSETS : DXL 중립(2048)이 대응하는 UR 관절각 [rad]
    const double default_offset[6] = { 0.0, -M_PI / 2.0, 0.0, -M_PI / 2.0, 0.0, 0.0 };

    for (int i = 0; i < 6; i++) {
        joint_direction_[i]  = default_dir[i];
        home_offset_rad_[i]  = default_offset[i];
        ema_q_[i]            = 0.0;
    }

    // ── config.yaml teleop: 섹션으로 오버라이드 ─────────────────────────────
    if (!config["teleop"])
        return;

    const auto& tc = config["teleop"];

    if (tc["ema_alpha"])
        ema_alpha_ = tc["ema_alpha"].as<double>();

    if (tc["joint_direction"] && tc["joint_direction"].IsSequence()) {
        int n = std::min(6, (int)tc["joint_direction"].size());
        for (int i = 0; i < n; i++)
            joint_direction_[i] = tc["joint_direction"][i].as<double>();
    }

    // home_offset_deg → rad 변환
    if (tc["home_offset_deg"] && tc["home_offset_deg"].IsSequence()) {
        int n = std::min(6, (int)tc["home_offset_deg"].size());
        for (int i = 0; i < n; i++)
            home_offset_rad_[i] = tc["home_offset_deg"][i].as<double>() * M_PI / 180.0;
    }
}

Teleop::~Teleop() = default;

// ============================================================================
// update — 500Hz 루프에서 호출
// ============================================================================

void Teleop::update(const MasterState& master, bool teleop_on)
{
    if (!teleop_on)
        return;

    URState cmd{};

    for (int i = 0; i < 6; i++) {
        // master.joint_angle[i] : DXL 엔코더 → rad (중립 = 0 기준)
        // 1) 축 방향 반전 + UR 홈 오프셋 적용
        double raw = master.joint_angle[i] * joint_direction_[i] + home_offset_rad_[i];

        // 2) EMA 저역통과 필터 (첫 샘플은 필터 없이 초기화)
        if (!ema_init_)
            ema_q_[i] = raw;
        ema_q_[i] = ema_alpha_ * raw + (1.0 - ema_alpha_) * ema_q_[i];

        cmd.joint_angle[i] = ema_q_[i];
    }
    ema_init_ = true;

    if (!ur_.writeJointAngles(cmd))
        std::cerr << "[Teleop] writeJointAngles failed\n";
}

// ============================================================================
// reset — EMA 필터 초기화 (텔레옵 재시작 시)
// ============================================================================

void Teleop::reset()
{
    ema_init_ = false;
    std::fill(std::begin(ema_q_), std::end(ema_q_), 0.0);
}
