#include <thread>
#include <atomic>
#include <csignal>
#include <yaml-cpp/yaml.h>

#include "common/common.h"
#include "hal/dynamixel/dynamixel_hal.h"
#include "hal/ur/ur_hal.h"
#include "gravity_compensation/gravity_compensation.h"
#include "teleop/teleop.h"
#include "recorder/recorder.h"

// ─── 종료 플래그 ─────────────────────────────────────
std::atomic<bool> running(true);

void signal_handler(int sig) {
    running = false;
}

// ─── 스레드 함수 선언 ────────────────────────────────
void gc_thread_func(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc);
void teleop_thread_func(SharedContext& ctx, URHal& ur);
void recorder_thread_func(SharedContext& ctx);
void input_thread_func(SharedContext& ctx);
void fsr_thread_func(SharedContext& ctx);

int main() {
    // ─── 시그널 등록 ─────────────────────────────────
    std::signal(SIGINT, signal_handler);

    // ─── config 로드 ─────────────────────────────────
    YAML::Node config = YAML::LoadFile("config/config.yaml");

    // ─── 객체 생성 ───────────────────────────────────
    SharedContext ctx;
    DynamixelHAL dynamixel(config);
    URHal ur(config);
    GravityCompensation gc(config);

    // ─── 초기화 ──────────────────────────────────────
    if (!dynamixel.init()) return -1;
    if (!ur.init())        return -1;
    if (!gc.init())        return -1;

    // ─── 스레드 생성 ─────────────────────────────────
    std::thread gc_thread(gc_thread_func, std::ref(ctx), std::ref(dynamixel), std::ref(gc));
    std::thread teleop_thread(teleop_thread_func, std::ref(ctx), std::ref(ur));
    std::thread recorder_thread(recorder_thread_func, std::ref(ctx));
    std::thread input_thread(input_thread_func, std::ref(ctx));
    std::thread fsr_thread(fsr_thread_func, std::ref(ctx));

    // ─── 종료 대기 ───────────────────────────────────
    gc_thread.join();
    teleop_thread.join();
    recorder_thread.join();
    input_thread.join();
    fsr_thread.join();

    // ─── 종료 처리 ───────────────────────────────────
    dynamixel.close();
    ur.close();

    return 0;
}


void gc_thread_func(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc) {
    // Xenomai RT 태스크 설정
    while (running) {
        // 1. dynamixel에서 master angle 읽기
        // 2. GC 계산 및 토크 출력 (FSR triggered 확인)
        // 3. ctx.master_state 업데이트 (mutex)
        // 4. 1kHz 주기 대기
    }
}

void teleop_thread_func(SharedContext& ctx, URHal& ur) {
    Teleop teleop(ur);
    while (running) {
        // 1. TELEOP 플래그 확인
        // 2. ctx.master_state 읽기 (mutex)
        // 3. teleop.update() 호출
        // 4. 500Hz 주기 대기
    }
}

void recorder_thread_func(SharedContext& ctx) {
    Recorder recorder;
    while (running) {
        {
            std::lock_guard<std::mutex> lock(ctx.flag_mutex);
            if (!hasFlag(ctx.system_flag, SystemFlag::SAVING)) {
                recorder.stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
        }

        // 처음 SAVING 켜질 때 파일 생성
        if (!recorder.isRecording()) {
            recorder.start();
        }

        // 데이터 읽기
        MasterState master;
        URState ur;
        {
            std::lock_guard<std::mutex> lock(ctx.master_mutex);
            master = ctx.master_state;
        }
        {
            std::lock_guard<std::mutex> lock(ctx.ur_mutex);
            ur = ctx.ur_state;
        }

        // 저장
        recorder.save(master, ur);

        // 50Hz 주기 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    recorder.stop();
}

void input_thread_func(SharedContext& ctx) {
    while (running) {
        char key = getchar();

        std::lock_guard<std::mutex> lock(ctx.flag_mutex);

        switch (key) {
            case 'T':
            case 't':
                if (hasFlag(ctx.system_flag, SystemFlag::TELEOP))
                    ctx.system_flag = clearFlag(ctx.system_flag, SystemFlag::TELEOP);
                else
                    ctx.system_flag = setFlag(ctx.system_flag, SystemFlag::TELEOP);
                break;

            case 'S':
            case 's':
                if (hasFlag(ctx.system_flag, SystemFlag::SAVING))
                    ctx.system_flag = clearFlag(ctx.system_flag, SystemFlag::SAVING);
                else
                    ctx.system_flag = setFlag(ctx.system_flag, SystemFlag::SAVING);
                break;

            default:
                break;
        }
    }
}

void fsr_thread_func(SharedContext& ctx) {
    while (running) {
        // 1. 라즈베리파이에서 FSR 데이터 수신 (TCP/UDP)
        // 2. 임계값 비교 후 triggered 판단
        // 3. ctx.fsr_state 업데이트 (mutex)
    }
}