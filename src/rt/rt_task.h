#pragma once
#include "common/common.h"
#include "hal/dynamixel/dynamixel_hal.h"
#include "gravity_compensation/gravity_compensation.h"

// ============================================================================
// RTTask — Xenomai Alchemy RT 태스크 래퍼
//
// gc_thread 를 Xenomai hard real-time 태스크로 실행한다.
// GravityCompensation / DynamixelHAL 은 플랫폼 독립 알고리즘으로 그대로 유지.
//
// 흐름 (62.5Hz):
//   rt_task_wait_period → readAngles → gc.update → writeCurrents
//   → ctx.master_state 업데이트 → 오버런/실패 카운트 출력 (1초마다)
//
// 사용:
//   RTTask rt(ctx, dynamixel, gc);
//   rt.start();   // RT 태스크 생성 + 시작
//   ...
//   rt.stop();    // 태스크 종료 대기
// ============================================================================

class RTTask {
public:
    RTTask(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc);
    ~RTTask();

    bool start();   // Xenomai RT 태스크 생성 및 시작
    void stop();    // 태스크 종료 대기

private:
    SharedContext&      ctx_;
    DynamixelHAL&       dynamixel_;
    GravityCompensation& gc_;

    static void taskEntry(void* arg);  // Xenomai 태스크 진입점 (정적)
    void        run();                  // 실제 RT 루프

    bool stopped_ = false;             // stop() 이중 호출 방지
};
