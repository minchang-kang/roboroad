#include <thread>
#include <atomic>
#include <csignal>
#include <sys/select.h>
#include <unistd.h>

#include "common/common.h"
#include "common/diag.h"
#include "hal/dynamixel/dynamixel_hal.h"
#include "hal/ur/ur_hal.h"
#include "hal/vision/vision_manager.h"
#include "hal/mouse/mouse_handler.h"
#include "gravity_compensation/gravity_compensation.h"
#include "teleop/teleop.h"
#include "save/save_manager.h"
#include "rt/rt_task.h"

// ─── 종료 플래그 ─────────────────────────────────────
std::atomic<bool> running(true);

void signal_handler(int sig) {
    running = false;
}

// ─── 스레드 함수 선언 ────────────────────────────────
void teleop_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config);
void rtde_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config);
void vision_thread_func(SharedContext& ctx, DiagContext& diag, const YAML::Node& config);
void recorder_thread_func(SharedContext& ctx, const YAML::Node& config);
void save_thread_func(SharedContext& ctx, DiagContext& diag, const YAML::Node& config);
void input_thread_func(SharedContext& ctx);
void mouse_thread_func(SharedContext& ctx, const YAML::Node& config);

int main() {
    // ─── 시그널 등록 ─────────────────────────────────
    std::signal(SIGINT, signal_handler);

    // ─── config 로드 ─────────────────────────────────
    YAML::Node config = YAML::LoadFile("config/config.yaml");

    // ─── 객체 생성 ───────────────────────────────────
    SharedContext ctx;
    DiagContext   diag;
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

    // ─── RT 태스크 시작 (Xenomai 250Hz GC) ───────────
    RTTask rt_task(ctx, dynamixel, gc);
    if (!rt_task.start()) return -1;

    // ─── 스레드 생성 ─────────────────────────────────
    std::thread teleop_thread(teleop_thread_func, std::ref(ctx), std::ref(ur), std::cref(config));
    std::thread rtde_thread(rtde_thread_func, std::ref(ctx), std::ref(ur), std::cref(config));
    std::thread vision_thread(vision_thread_func, std::ref(ctx), std::ref(diag), std::cref(config));
    std::thread recorder_thread(recorder_thread_func, std::ref(ctx), std::cref(config));
    std::thread save_thread(save_thread_func, std::ref(ctx), std::ref(diag), std::cref(config));
    std::thread input_thread(input_thread_func, std::ref(ctx));
    std::thread mouse_thread(mouse_thread_func, std::ref(ctx), std::cref(config));

    // ─── 종료 대기 ───────────────────────────────────
    rt_task.stop();
    teleop_thread.join();
    rtde_thread.join();
    vision_thread.join();
    recorder_thread.join();
    save_thread.join();
    input_thread.join();
    mouse_thread.join();

    // ─── 종료 처리 ───────────────────────────────────
    dynamixel.close();
    ur.close();

    return 0;
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

        switch (key) {
            case 'T':
            case 't':
                ctx.teleop_on.store(!ctx.teleop_on.load());
                std::cout << "[input] TELEOP " << (ctx.teleop_on ? "ON" : "OFF") << std::endl;
                break;

            case 'S':
            case 's':
                {
                    std::lock_guard<std::mutex> lock(ctx.save_mutex);
                    ctx.saving = !ctx.saving;
                    std::cout << "[input] SAVING " << (ctx.saving ? "ON" : "OFF") << std::endl;
                }
                ctx.save_cv.notify_one();
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

        // 1. master_state 스냅샷
        MasterState master;
        {
            std::shared_lock lock(ctx.master_mutex);
            master = ctx.master_state;
        }

        // 2. TELEOP ON이면 UR에 servoJ 명령 전송
        teleop.update(master, ctx.teleop_on.load());
    }
}

void vision_thread_func(SharedContext& ctx, DiagContext& diag, const YAML::Node& config) {
    VisionManager vision_manager(config, ctx);
    vision_manager.openAll();

    std::vector<std::thread> cam_threads;
    for (auto& [role, queue] : ctx.vision_queues) {
        cam_threads.emplace_back([&ctx, &diag, &vision_manager, role = role]() {
            Vision& cam = vision_manager.get(role);
            while (running) {
                FrameData fd;
                if (cam.read(fd.frame)) {
                    fd.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                    ctx.vision_queues[role]->push(std::move(fd));
                } else {
                    bool saving;
                    {
                        std::lock_guard<std::mutex> lock(ctx.save_mutex);
                        saving = ctx.saving;
                    }
                    if (saving)
                        diag.record_drop(role);
                }
            }
        });
    }

    for (auto& t : cam_threads) t.join();
    vision_manager.releaseAll();
}

void recorder_thread_func(SharedContext& ctx, const YAML::Node& config) {
    const int recorder_hz = config["timing"]["recorder_loop_hz"].as<int>();
    auto next = std::chrono::steady_clock::now();
    const auto interval = std::chrono::milliseconds(1'000 / recorder_hz);
 
    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;
 
        uint64_t ref_ts = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
 
        MasterState master;
        URState     ur;
 
        // 큐에서 현재 시간(ref_ts)에 가장 가까운 데이터를 복사 (큐에서 제거하지 않음)
        bool has_master = ctx.master_queue.peek_closest(ref_ts, master);
        bool has_ur     = ctx.ur_queue.peek_closest(ref_ts, ur);
 
        // 만약 큐가 비어 있다면 실시간 최신 상태값으로 Fallback 처리
        if (!has_master) {
            std::shared_lock lock(ctx.master_mutex);
            master = ctx.master_state;
        }
        if (!has_ur) {
            std::shared_lock lock(ctx.ur_mutex);
            ur = ctx.ur_state;
        }
 
        SaveData sd;
        sd.timestamp_us = ref_ts;
        sd.master = master;
        sd.ur     = ur;
        {
            std::lock_guard<std::mutex> lock(ctx.mouse_mutex);
            sd.mouse = ctx.mouse_state;
        }

        // 비전: ref_ts에 가장 가까운 프레임을 복사 (큐에서 제거하지 않음)
        for (auto& [role, queue] : ctx.vision_queues) {
            queue->peek_closest(ref_ts, sd.frames[role]);
            if (!sd.frames[role].frame.empty()) {
                cv::Mat rgb;
                cv::cvtColor(sd.frames[role].frame, rgb, cv::COLOR_BGR2RGB);
                sd.frames[role].frame = std::move(rgb);
            }
        }

        // SAVING ON일 때만 HDF5 큐에 추가 (saving 체크 + push를 save_mutex로 통합)
        {
            std::lock_guard<std::mutex> lock(ctx.save_mutex);
            if (!ctx.saving) continue;
            ctx.save_queue.push(std::move(sd));
        }
        ctx.save_cv.notify_one();
    }
 
    ctx.save_cv.notify_all();
}


void save_thread_func(SharedContext& ctx, DiagContext& diag, const YAML::Node& config) {
    SaveManager save_manager(config);

    while (running) {
        std::unique_lock<std::mutex> lock(ctx.save_mutex);

        // 1. 세션 시작 대기 (saving ON or 종료)
        ctx.save_cv.wait(lock, [&] {
            return ctx.saving || !running;
        });
        if (!running) break;

        // 2. 파일 열기
        diag.reset();
        lock.unlock();
        save_manager.start();
        lock.lock();

        uint64_t write_sum_us = 0, write_min_us = UINT64_MAX, write_max_us = 0;
        uint64_t write_count = 0;

        // 3. 세션 처리
        while (running) {
            ctx.save_cv.wait(lock, [&] {
                return !ctx.save_queue.empty() || !ctx.saving || !running;
            });

            if (!ctx.saving && ctx.save_queue.empty()) break;

            std::vector<SaveData> batch;
            while (!ctx.save_queue.empty()) {
                batch.push_back(std::move(ctx.save_queue.front()));
                ctx.save_queue.pop();
            }
            lock.unlock();

            if (!batch.empty()) {
                auto t0 = std::chrono::steady_clock::now();
                save_manager.saveBatch(batch);
                auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                write_sum_us += elapsed_us;
                write_min_us  = std::min(write_min_us, (uint64_t)elapsed_us);
                write_max_us  = std::max(write_max_us, (uint64_t)elapsed_us);
                write_count  += batch.size();
            }

            lock.lock();
        }

        // 4. 파일 닫기
        lock.unlock();
        save_manager.stop();
        diag.report();

        if (write_count > 0) {
            std::cout << "[save_thread] write 성능 | "
                      << "avg=" << write_sum_us / write_count / 1000 << "ms "
                      << "min=" << write_min_us / 1000 << "ms "
                      << "max=" << write_max_us / 1000 << "ms "
                      << "frames=" << write_count << "\n";
        }
    }
}

void rtde_thread_func(SharedContext& ctx, URHal& ur, const YAML::Node& config) {
    const int rtde_hz = config["timing"]["rtde_loop_hz"].as<int>();
    const auto interval = std::chrono::microseconds(1'000'000 / rtde_hz);
    auto next = std::chrono::steady_clock::now();

    while (running) {
        std::this_thread::sleep_until(next);
        next += interval;

        URState state{};
        if (!ur.readJointAngles(state))
            continue;

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

void mouse_thread_func(SharedContext& ctx, const YAML::Node& config) {
    MouseHandler mouse(config);
    if (!mouse.init()) return;
    mouse.run(ctx, running);
    mouse.close();
}
