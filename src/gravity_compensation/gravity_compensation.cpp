// ============================================================================
// gravity_compensation.cpp
//
// RL(Robotics Library) 기반 6축 중력 보상 구현
//
// 레퍼런스: /home/mugun/Desktop/roboroad/src/rt_task.cpp
//   rt_gravity_task() 의 중력 보상 파이프라인을 클래스로 이식
//
// 파이프라인:
//   joint_angle[rad] → RL calculateGravity → tau[Nm]
//   → 방향 적용 → 게인·전류 변환 → clamp → goal_cur[raw]
// ============================================================================

#include "gravity_compensation/gravity_compensation.h"

#include <rl/mdl/Dynamic.h>
#include <rl/mdl/UrdfFactory.h>
#include <rl/math/Vector.h>

#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

// ============================================================================
// 생성자
// ============================================================================

GravityCompensation::GravityCompensation(const YAML::Node& config)
    : urdf_path_(config["gc"]["urdf_path"].as<std::string>())
    , gravity_gain_(1.0)
{
    // ── 기본값 (레퍼런스 config.h 기준) ────────────────────────────────────
    const double  default_dir[6]  = { 1.0,   -1.0,  -1.0,   1.0,   1.0,   1.0  };
    const double  default_gain[6] = { 0.8,    0.8,   0.8,   0.8,   0.8,   0.8  };
    const double  default_kt[6]   = { 1.37,   1.37,  1.30,  1.30,  1.30,  1.30 };
    const int16_t default_lim[6]  = { 1000,   1000,  800,   800,   800,   800  };

    for (int i = 0; i < 6; i++) {
        joint_direction_[i] = default_dir[i];
        current_gain_[i]    = default_gain[i];
        kt_[i]              = default_kt[i];
        cur_limit_[i]       = default_lim[i];
        goal_cur_[i]        = 0;
    }

    // ── config.yaml gc: 섹션으로 오버라이드 ─────────────────────────────
    const auto& gc = config["gc"];

    if (gc["gravity_gain"])
        gravity_gain_ = gc["gravity_gain"].as<double>();

    auto load_double6 = [&](const char* key, double out[6]) {
        if (gc[key] && gc[key].IsSequence()) {
            int n = std::min(6, (int)gc[key].size());
            for (int i = 0; i < n; i++)
                out[i] = gc[key][i].as<double>();
        }
    };
    auto load_int16_6 = [&](const char* key, int16_t out[6]) {
        if (gc[key] && gc[key].IsSequence()) {
            int n = std::min(6, (int)gc[key].size());
            for (int i = 0; i < n; i++)
                out[i] = static_cast<int16_t>(gc[key][i].as<int>());
        }
    };

    load_double6("joint_direction", joint_direction_);
    load_double6("current_gain",    current_gain_);
    load_double6("kt",              kt_);
    load_int16_6("cur_limit",       cur_limit_);
}

// destructor: unique_ptr<rl::mdl::Dynamic> 완전 타입 필요 → .cpp 에서 정의
GravityCompensation::~GravityCompensation() = default;

// ============================================================================
// init — URDF 로드 및 RL 동역학 모델 초기화
// ============================================================================

bool GravityCompensation::init()
{
    try {
        rl::mdl::UrdfFactory factory;
        model_ = std::dynamic_pointer_cast<rl::mdl::Dynamic>(factory.create(urdf_path_));

        if (!model_) {
            std::cerr << "[GC] URDF 로드 실패 또는 Dynamic 모델이 아님: "
                      << urdf_path_ << std::endl;
            return false;
        }

        if (static_cast<int>(model_->getDof()) != 6) {
            std::cerr << "[GC] DoF 불일치: 기대 6, 실제 "
                      << model_->getDof() << std::endl;
            return false;
        }

        std::cout << "[GC] RL 모델 로드 완료: " << urdf_path_
                  << "  DoF=" << model_->getDof() << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[GC] init 예외: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// update — 중력 보상 계산
//   레퍼런스 rt_gravity_task() 의 스텝 2~4 를 그대로 이식
// ============================================================================

void GravityCompensation::update(MasterState& master)
{
    if (!model_)
        return;

    // ── 1. 관절각도 → RL 좌표계 변환 ───────────────────────────────────
    // master.joint_angle[i] : DXL 엔코더 → rad (중립 = 0 기준)
    // joint_direction_[i]   : DXL 축 방향을 URDF 방향으로 맞춤
    rl::math::Vector q(6);
    for (int i = 0; i < 6; i++)
        q[i] = master.joint_angle[i] * joint_direction_[i];

    // ── 2. RL: 중력 보상 토크 계산 ──────────────────────────────────────
    rl::math::Vector tau(6);
    model_->setPosition(q);
    model_->calculateGravity(tau);

    // ── 3. 토크 → 목표 전류 변환 ────────────────────────────────────────
    // 레퍼런스 rt_task.cpp 변환식:
    //   raw = (tau_cmd / Kt) * 1000[mA/A] / 2.69[mA/raw] * current_gain
    static constexpr double DXL_CUR_UNIT_MA = 2.69;

    for (int i = 0; i < 6; i++) {
        double tau_raw = tau[i];
        double tau_cmd = tau_raw * gravity_gain_ * joint_direction_[i];

        double raw_d = (tau_cmd / kt_[i]) * 1000.0 / DXL_CUR_UNIT_MA;
        raw_d *= current_gain_[i];

        int16_t raw = static_cast<int16_t>(raw_d);
        if (raw >  cur_limit_[i]) raw =  cur_limit_[i];
        if (raw < -cur_limit_[i]) raw = -cur_limit_[i];

        goal_cur_[i]       = raw;
        master.torque[i]   = tau_cmd;   // Nm — 로그/저장용
    }
}
