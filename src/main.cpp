#include <thread>
#include <atomic>
#include <csignal>
#include <sys/select.h>
#include <unistd.h>

#include "common/common.h"
#include "hal/dynamixel/dynamixel_hal.h"
// #include "hal/fsr"
#include "hal/ur/ur_hal.h"
#include "hal/vision/vision_manager.h"
#include "gravity_compensation/gravity_compensation.h"
#include "teleop/teleop.h"
#include "save/save_manager.h"

// ─── 종료 플래그 ─────────────────────────────────────
std::atomic<bool> running(true);

void signal_handler(int sig) {
    running = false;
}

// ─── 스레드 함수 선언 ────────────────────────────────
void gc_thread_func(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc);
void teleop_thread_func(SharedContext& ctx, URHal& ur);
void rtde_thread_func(SharedContext& ctx, URHal& ur);
void vision_thread_func(SharedContext& ctx, const YAML::Node& config);
void recorder_thread_func(SharedContext& ctx);
void save_thread_func(SharedContext& ctx, const YAML::Node& config);
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
    std::thread rtde_thread(rtde_thread_func, std::ref(ctx), std::ref(ur));
    std::thread vision_thread(vision_thread_func, std::ref(ctx), std::cref(config));
    std::thread recorder_thread(recorder_thread_func, std::ref(ctx));
    std::thread save_thread(save_thread_func, std::ref(ctx), std::cref(config));
    std::thread input_thread(input_thread_func, std::ref(ctx));
    std::thread fsr_thread(fsr_thread_func, std::ref(ctx));

    // ─── 종료 대기 ───────────────────────────────────
    gc_thread.join();
    teleop_thread.join();
    rtde_thread.join();
    vision_thread.join();
    recorder_thread.join();
    save_thread.join();
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
    auto next = std::chrono::steady_clock::now();
    const auto interval = std::chrono::milliseconds(20); // 50Hz

    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;

        {
            std::lock_guard<std::mutex> lock(ctx.flag_mutex);
            if (!hasFlag(ctx.system_flag, SystemFlag::SAVING))
                continue;
        }

        uint64_t ref_ts = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        SaveData sd;
        sd.timestamp_us = ref_ts;
        {
            std::shared_lock lock(ctx.master_mutex);
            sd.master = ctx.master_state;
        }
        {
            std::shared_lock lock(ctx.ur_mutex);
            sd.ur = ctx.ur_state;
        }
        for (auto& [role, queue] : ctx.vision_queues)
            queue->pop_closest(ref_ts, sd.frames[role]);

        {
            std::lock_guard<std::mutex> lock(ctx.save_mutex);
            ctx.save_queue.push(std::move(sd));
        }
        ctx.save_cv.notify_one();
    }

    ctx.save_cv.notify_all(); // save_thread 종료 깨우기
}

void save_thread_func(SharedContext& ctx, const YAML::Node& config) {
    SaveManager save_manager(config);

    while (true) {
        std::unique_lock<std::mutex> lock(ctx.save_mutex);
        ctx.save_cv.wait(lock, [&] {
            return !ctx.save_queue.empty() || !running;
        });

        while (!ctx.save_queue.empty()) {
            SaveData sd = std::move(ctx.save_queue.front());
            ctx.save_queue.pop();
            lock.unlock();

            if (!save_manager.isRecording())
                save_manager.start();
            save_manager.save(sd);

            lock.lock();
        }

        // 큐가 비었을 때 SAVING OFF면 파일 닫기
        {
            std::lock_guard<std::mutex> flag_lock(ctx.flag_mutex);
            if (!hasFlag(ctx.system_flag, SystemFlag::SAVING))
                save_manager.stop();
        }

        if (!running && ctx.save_queue.empty())
            break;
    }

    save_manager.stop();
}

void input_thread_func(SharedContext& ctx) {
    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 100000}; // 100ms 타임아웃
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
            continue;

        char key = getchar();

        std::lock_guard<std::mutex> lock(ctx.flag_mutex);

        switch (key) {
            case 'T':
            case 't':
                if (hasFlag(ctx.system_flag, SystemFlag::TELEOP)) {
                    ctx.system_flag = clearFlag(ctx.system_flag, SystemFlag::TELEOP);
                    std::cout << "[input] TELEOP OFF" << std::endl;
                } else {
                    ctx.system_flag = setFlag(ctx.system_flag, SystemFlag::TELEOP);
                    std::cout << "[input] TELEOP ON" << std::endl;
                }
                break;

            case 'S':
            case 's':
                if (hasFlag(ctx.system_flag, SystemFlag::SAVING)) {
                    ctx.system_flag = clearFlag(ctx.system_flag, SystemFlag::SAVING);
                    std::cout << "[input] SAVING OFF" << std::endl;
                } else {
                    ctx.system_flag = setFlag(ctx.system_flag, SystemFlag::SAVING);
                    std::cout << "[input] SAVING ON" << std::endl;
                }
                break;

            default:
                break;
        }
    }
}

void rtde_thread_func(SharedContext& ctx, URHal& ur) {
    while (running) {
        // 1. ur에서 joint angle 읽기
        // 2. ctx.ur_state 업데이트 (unique_lock)
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); // 500Hz
    }
}

void vision_thread_func(SharedContext& ctx, const YAML::Node& config) {
    VisionManager vision_manager(config, ctx);
    vision_manager.openAll();

    std::vector<std::thread> cam_threads;
    for (auto& [role, queue] : ctx.vision_queues) {
        cam_threads.emplace_back([&ctx, &vision_manager, role = role]() {
            Vision& cam = vision_manager.get(role);
            while (running) {
                FrameData fd;
                if (cam.read(fd.frame)) {
                    fd.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    ctx.vision_queues[role]->push(std::move(fd));
                }
            }
        });
    }

    for (auto& t : cam_threads) t.join();
    vision_manager.releaseAll();
}

void fsr_thread_func(SharedContext& ctx) {
    while (running) {

    }
}