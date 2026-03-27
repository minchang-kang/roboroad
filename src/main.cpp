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
#include "save/log_manager.h"
#include "rt/rt_task.h"

// ─── 종료 플래그 ─────────────────────────────────────
std::atomic<bool> running(true);

void signal_handler(int sig) {
    running = false;
}

// ─── 스레드 함수 선언 ────────────────────────────────
void teleop_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config);
void rtde_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config);
void vision_thread_func(SharedContext& ctx, const YAML::Node& config);
void recorder_thread_func(SharedContext& ctx, const YAML::Node& config);
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

    // ─── UR 초기화 ──────────────────────────────────────
    if (config["ur"]["auto_home"].as<bool>(false))
        ur.moveToHome();

    // ─── RT 태스크 시작 (Xenomai 1kHz GC) ───────────
    RTTask rt_task(ctx, dynamixel, gc);
    if (!rt_task.start()) return -1;

    // ─── 스레드 생성 ─────────────────────────────────
    std::thread teleop_thread(teleop_thread_func, std::ref(ctx), std::ref(ur), std::cref(config));
    std::thread rtde_thread(rtde_thread_func, std::ref(ctx), std::ref(ur), std::cref(config));
    std::thread vision_thread(vision_thread_func, std::ref(ctx), std::cref(config));
    std::thread recorder_thread(recorder_thread_func, std::ref(ctx), std::cref(config));
    std::thread save_thread(save_thread_func, std::ref(ctx), std::cref(config));
    std::thread input_thread(input_thread_func, std::ref(ctx));
    std::thread fsr_thread(fsr_thread_func, std::ref(ctx));

    // ─── 종료 대기 ───────────────────────────────────
    rt_task.stop();
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


// void gc_thread_func(SharedContext& ctx, DynamixelHAL& dynamixel, GravityCompensation& gc) {
//     const auto interval = std::chrono::microseconds(1000); // 1kHz
//     auto next = std::chrono::steady_clock::now();

//     while (running) {
//         std::this_thread::sleep_until(next);
//         next += interval;

//         MasterState master{};

//         // 1. DXL에서 관절각 읽기
//         if (!dynamixel.readAngles(master))
//             continue;

//         // 2. GC 계산 — master.torque[6] (Nm) + 내부 goal_cur_[6] (raw LSB) 채움
//         gc.update(master);

//         // 3. DXL에 목표 전류 전송
//         dynamixel.writeCurrents(gc.getGoalCurrents());

//         // 4. ctx.master_state 업데이트
//         master.timestamp_us = static_cast<uint64_t>(
//             std::chrono::duration_cast<std::chrono::microseconds>(
//                 std::chrono::steady_clock::now().time_since_epoch()).count());
//         {
//             std::unique_lock lock(ctx.master_mutex);
//             ctx.master_state = master;
//         }
//     }

//     dynamixel.setTorqueEnable(false);
// }

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

void teleop_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config) {
    Teleop teleop(ur, config);
    const int teleop_hz = config["timing"]["teleop_loop_hz"].as<int>();
    const auto interval = std::chrono::microseconds(1'000'000 / teleop_hz);
    auto next = std::chrono::steady_clock::now();

    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;

        // 1. 플래그 스냅샷
        SystemFlag flag;
        {
            std::lock_guard<std::mutex> lock(ctx.flag_mutex);
            flag = ctx.system_flag;
        }

        // 2. master_state 스냅샷
        MasterState master;
        {
            std::shared_lock lock(ctx.master_mutex);
            master = ctx.master_state;
        }

        // 3. TELEOP ON이면 UR에 servoJ 명령 전송
        teleop.update(master, flag);
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

void recorder_thread_func(SharedContext& ctx, const YAML::Node& config) {
    LogManager log(config);
    if (!log.open()) return;

    const int recorder_hz = config["timing"]["recorder_loop_hz"].as<int>();
    auto next = std::chrono::steady_clock::now();
    const auto interval = std::chrono::milliseconds(1'000 / recorder_hz); // 50Hz

    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;

        uint64_t ref_ts = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        MasterState master;
        URState     ur;

        // 큐에서 현재 시간(ref_ts)에 가장 가까운 데이터를 팝업하여 동기화
        bool has_master = ctx.master_queue.pop_closest(ref_ts, master);
        bool has_ur = ctx.ur_queue.pop_closest(ref_ts, ur);

        // 만약 큐가 비어 있다면 실시간 최신 상태값으로 Fallback 처리
        if (!has_master) {
            std::shared_lock lock(ctx.master_mutex);
            master = ctx.master_state;
        }
        if (!has_ur) {
            std::shared_lock lock(ctx.ur_mutex);
            ur = ctx.ur_state;
        }

        // CSV 항상 기록
        log.write(master, ur);

        // SAVING ON일 때만 HDF5 큐에 추가
        {
            std::lock_guard<std::mutex> lock(ctx.flag_mutex);
            if (!hasFlag(ctx.system_flag, SystemFlag::SAVING))
                continue;
        }

        SaveData sd;
        sd.timestamp_us = ref_ts;
        sd.master = master;
        sd.ur     = ur;
        for (auto& [role, queue] : ctx.vision_queues)
            queue->pop_closest(ref_ts, sd.frames[role]);

        {
            std::lock_guard<std::mutex> lock(ctx.save_mutex);
            ctx.save_queue.push(std::move(sd));
        }
        ctx.save_cv.notify_one();
    }

    log.close();
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

void rtde_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config) {
    const int rtde_hz = config["timing"]["rtde_loop_hz"].as<int>();
    const auto interval = std::chrono::microseconds(1'000'000 / rtde_hz); // 500Hz
    auto next = std::chrono::steady_clock::now();

    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;

        // 1. UR에서 관절각 읽기
        URState state{};
        if (!ur.readJointAngles(state))
            continue;

        // 2. ctx.ur_state 업데이트
        {
            std::unique_lock lock(ctx.ur_mutex);
            ctx.ur_state = state;
        }

        if (state.timestamp_us == 0) {
            state.timestamp_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }
        ctx.ur_queue.push(state);
    }
}

void fsr_thread_func(SharedContext& ctx) {
    while (running) {

    }
}
