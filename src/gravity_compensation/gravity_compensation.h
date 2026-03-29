#pragma once
#include "common/common.h"
#include <string>
#include <memory>

// Forward declaration — 무거운 RL 헤더를 .h 에서 숨김 (.cpp 에서만 include)
namespace rl { namespace mdl { class Dynamic; } }

// ============================================================================
// GravityCompensation
//
// RL(Robotics Library) 동역학 모델을 이용해 6축 중력 보상 토크를 계산한다.
//
// 흐름 (gc_thread 250Hz):
//   1. update(master) — joint_angle[6] 입력
//   2. RL setPosition → calculateGravity → 토크 [Nm]
//   3. 토크 → 목표 전류 변환 (Kt, gain, clamp)
//   4. master.torque[6] 에 결과 저장 (Nm, 로그/저장용)
//   5. getGoalCurrents() — DynamixelHAL 이 전류 쓰기에 사용
//
// config.yaml 에서 읽는 파라미터 (gc: 섹션):
//   urdf_path        : RL 모델용 URDF 파일 경로
//   gravity_gain     : 전체 게인 스케일 (기본 1.0)
//   joint_direction  : [±1 × 6] — DXL CCW = URDF + 방향이면 +1
//   current_gain     : [0~1 × 6] — 축별 안전 게인
//   kt               : [Nm/A × 6] — 모터 토크 상수
//   cur_limit        : [raw × 6] — 축별 전류 한계
// ============================================================================

class GravityCompensation {
public:
    GravityCompensation(const YAML::Node& config);
    ~GravityCompensation();  // unique_ptr 완전 타입 필요 → .cpp 에서 정의

    // URDF 로드 및 RL 모델 초기화
    bool init();

    // GC 계산 — master.joint_angle 읽어서 master.torque 채움
    // 목표 전류는 내부 저장 → getGoalCurrents() 로 조회
    void update(MasterState& master);

    // DynamixelHAL 이 전류 쓰기에 사용하는 목표 전류 [raw LSB]
    const int16_t* getGoalCurrents() const { return goal_cur_; }

private:
    std::string urdf_path_;

    // GC 파라미터 (레퍼런스 config.h 기준 기본값)
    double  joint_direction_[6];   // +1 or -1
    double  current_gain_[6];      // 안전 게인 (0~1)
    double  kt_[6];                // 토크 상수 Kt [Nm/A]
    int16_t cur_limit_[6];         // 전류 한계 [raw LSB]
    double  gravity_gain_[6];         // 전체 게인 (기본 1.0)

    // RL 동역학 모델 (factory.create() 가 shared_ptr 반환)
    std::shared_ptr<rl::mdl::Dynamic> model_;

    // 최종 목표 전류 [raw LSB] — update() 마다 갱신
    int16_t goal_cur_[6];
};
