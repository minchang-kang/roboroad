// ============================================================================
// rt_task.cpp
//
// Xenomai Alchemy RT 태스크 — 250Hz GC 루프
// ============================================================================

#include "rt/rt_task.h"

#include <alchemy/task.h>
#include <trank/rtdk.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstdio>
#include <chrono>
#include <atomic>

// 외부 종료 플래그 (main.cpp 의 running 과 공유)
extern std::atomic<bool> running;

// Xenomai 태스크 핸들 (프로세스 당 하나)
static RT_TASK g_rt_task;

// RT 루프 주기: 62.5Hz = 16,000,000 ns
static constexpr RTIME RT_PERIOD_NS = 16'000'000ULL;

// RT 태스크 우선순위 (1~99, 높을수록 우선)
static constexpr int RT_PRIORITY = 90;

// ============================================================================
// 생성자 / 소멸자
// ============================================================================

RTTask::RTTask(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc)
    : ctx_(ctx), dynamixel_(dynamixel), gc_(gc)
{}

RTTask::~RTTask()
{
    stop();
}

// ============================================================================
// start — mlockall + rt_task_create + rt_task_start
// ============================================================================

bool RTTask::start()
{    // 메모리 페이지 잠금: RT 태스크에서 페이지 폴트 방지    
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
    {
        perror("[RTTask] mlockall 실패");
        return false;
    }

    int ret = rt_task_create(&g_rt_task, "gc_rt", 0, RT_PRIORITY, T_JOINABLE);    
    if (ret != 0) 
    {        
        fprintf(stderr, "[RTTask] rt_task_create 실패: %d\n", ret);
        return false;
    }

    // CPU 10번에 RT 태스크 고정 (isolcpus=10,11 에서 10번 사용)    
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(10, &cpus);

    ret = rt_task_set_affinity(&g_rt_task, &cpus);    
    if (ret != 0) 
    {        
        fprintf(stderr, "[RTTask] rt_task_set_affinity 실패: %d\n", ret);
    } 
    else 
    {
        printf("[RTTask] CPU affinity → CPU 10\n");
    }

    ret = rt_task_start(&g_rt_task, &RTTask::taskEntry, this);
    if (ret != 0)
    {
        fprintf(stderr, "[RTTask] rt_task_start 실패: %d\n", ret);
        rt_task_delete(&g_rt_task);
        return false;
    }

    printf("[RTTask] RT 태스크 시작 (62.5Hz, priority=%d)\n", RT_PRIORITY);
    return true;
}

// ============================================================================
// stop — 태스크 종료 대기 후 삭제
// ============================================================================

void RTTask::stop()
{
    if (stopped_) return;
    stopped_ = true;
    rt_task_join(&g_rt_task);
    rt_task_delete(&g_rt_task);
    printf("[RTTask] RT 태스크 종료\n");
}

// ============================================================================
// taskEntry — Xenomai 스케줄러가 호출하는 정적 진입점
// ============================================================================

void RTTask::taskEntry(void* arg)
{
    static_cast<RTTask*>(arg)->run();
}

// ============================================================================
// run — 실제 250Hz RT 루프
// ============================================================================

void RTTask::run()
{
    rt_printf("[RTTask] 루프 시작 (62.5Hz | SyncRead → GC → SyncWrite)\n");

    uint64_t cycle    = 0;
    uint64_t overruns = 0;
    uint64_t rfail    = 0;
    uint64_t wfail    = 0;

    rt_task_set_periodic(NULL, TM_NOW, RT_PERIOD_NS);

    while (running) {
        unsigned long overrun_count = 0;
        int ret = rt_task_wait_period(&overrun_count);

        if (ret != 0 && ret != -ETIMEDOUT) {
            if (ret == -EINTR) break;
            continue;
        }
        overruns += overrun_count;
        ++cycle;

        MasterState master{};

        // 1. DXL 관절각 읽기
        if (!dynamixel_.readAngles(master)) {
            ++rfail;
            // 읽기 실패 시 전류 0 전송 (안전)
            int16_t zero[6] = {};
            dynamixel_.writeCurrents(zero);
            continue;
        }

        // 2. GC 계산 — master.torque[6] (Nm) + goal_cur_[6] (raw LSB) 채움
        gc_.update(master);

        // 3. DXL 목표 전류 전송
        if (!dynamixel_.writeCurrents(gc_.getGoalCurrents()))
            ++wfail;

        // 4. ctx.master_state 업데이트
        master.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        {
            std::unique_lock lock(ctx_.master_mutex);
            ctx_.master_state = master;
        }
        ctx_.master_queue.push(master);

        // 5. 1초마다 RT-safe 콘솔 출력 (62.5Hz 기준 63 사이클)
        if (cycle % 63 == 0) {
            rt_printf("[RTTask] cycle:%-8lu overrun:%-4lu rfail:%-4lu wfail:%-4lu\n",
                      (unsigned long)cycle,
                      (unsigned long)overruns,
                      (unsigned long)rfail,
                      (unsigned long)wfail);
            rt_printf("  q[rad] J1:%+.4f J2:%+.4f J3:%+.4f J4:%+.4f J5:%+.4f J6:%+.4f\n",
                      master.joint_angle[0], master.joint_angle[1], master.joint_angle[2],
                      master.joint_angle[3], master.joint_angle[4], master.joint_angle[5]);
            rt_printf("  tau[Nm] J1:%+.4f J2:%+.4f J3:%+.4f J4:%+.4f J5:%+.4f J6:%+.4f\n",
                      master.torque[0], master.torque[1], master.torque[2],
                      master.torque[3], master.torque[4], master.torque[5]);
        }
    }

    // 종료 시 전류 0
    int16_t zero[6] = {};
    dynamixel_.writeCurrents(zero);
    dynamixel_.setTorqueEnable(false);
    rt_printf("[RTTask] 루프 종료\n");
}
