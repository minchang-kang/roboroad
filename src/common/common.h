#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <queue>
#include <condition_variable>
#include <map>
#include <string>
#include <memory>
#include <yaml-cpp/yaml.h>
#include <opencv2/opencv.hpp>

// ─── 비전 프레임 큐 ──────────────────────────────────

struct FrameData {
    cv::Mat        frame;
    uint64_t       timestamp_us;
};

template <typename T>
struct DataQueue {
    std::deque<T> data;
    mutable std::mutex    mutex;
    size_t                max_size;

    explicit DataQueue(size_t max_size_) : max_size(max_size_) {}

    void push(T item) {
        std::lock_guard<std::mutex> lock(mutex);
        if (data.size() >= max_size)
            data.pop_front();
        data.push_back(std::move(item));
    }

    // ref_ts에 가장 가까운 데이터를 복사해서 반환 (큐에서 제거하지 않음)
    bool peek_closest(uint64_t ref_ts, T& out) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (data.empty()) return false;

        size_t best_idx = 0;
        uint64_t best_diff = data[0].timestamp_us > ref_ts
            ? data[0].timestamp_us - ref_ts
            : ref_ts - data[0].timestamp_us;

        for (size_t i = 1; i < data.size(); ++i) {
            uint64_t diff = data[i].timestamp_us > ref_ts
                ? data[i].timestamp_us - ref_ts
                : ref_ts - data[i].timestamp_us;
            if (diff < best_diff) {
                best_diff = diff;
                best_idx  = i;
            }
        }

        out = data[best_idx];  // copy
        return true;
    }

    // ref_ts에 가장 가까운 데이터 반환, 그보다 오래된 데이터는 버림
    bool pop_closest(uint64_t ref_ts, T& out) {
        std::lock_guard<std::mutex> lock(mutex);
        if (data.empty()) return false;

        while (data.size() > 1) {
            uint64_t d0 = data[0].timestamp_us > ref_ts ? data[0].timestamp_us - ref_ts
                                                        : ref_ts - data[0].timestamp_us;
            uint64_t d1 = data[1].timestamp_us > ref_ts ? data[1].timestamp_us - ref_ts
                                                        : ref_ts - data[1].timestamp_us;
            if (d1 <= d0)
                data.pop_front();
            else
                break;
        }

        out = std::move(data.front());
        data.pop_front();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return data.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return data.size();
    }
};

// 기존 코드 호환성을 위한 Alias
using VisionQueue = DataQueue<FrameData>;


// ─── 데이터 구조 ─────────────────────────────────────

// 마스터 디바이스 상태 (Dynamixel에서 읽음)
struct MasterState {
    double joint_angle[6];      // 조인트 각도 (rad)
    double torque[6];           // 토크 출력값 (GC 계산 결과)
    uint64_t timestamp_us;      // 타임스탬프
};

struct URState {
    double joint_angle[6];      // 조인트 각도 (rad)
    uint64_t timestamp_us;
};

struct MouseState {
    uint8_t  pressed      = 0;   // 1: 좌클릭 누름, 0: 때짐
    uint64_t timestamp_us = 0;   // 이벤트 발생 시각 [μs]
};


// ─── 저장 데이터 ──────────────────────────────────────

struct SaveData {
    MasterState                      master;
    URState                          ur;
    MouseState                       mouse;
    std::map<std::string, FrameData> frames;  // role → frame
    uint64_t                         timestamp_us;
};


// ─── 공유 컨텍스트 ───────────────────────────────────

struct SharedContext {
    MasterState master_state;
    URState     ur_state;

    // 단순 on/off 플래그 → lock-free atomic
    std::atomic<bool> teleop_on{false};
    std::atomic<bool> spray_on{false};

    mutable std::shared_mutex master_mutex;
    mutable std::shared_mutex ur_mutex;

    MouseState mouse_state;
    std::mutex mouse_mutex;

    std::map<std::string, std::unique_ptr<VisionQueue>> vision_queues;
    DataQueue<MasterState> master_queue{1000}; // 1초 분량 (250Hz 버퍼)
    DataQueue<URState>     ur_queue{500};      // 1초 분량 (500Hz 버퍼)

    // SAVING은 save_queue와 함께 save_mutex로 보호
    bool                    saving = false;
    std::queue<SaveData>    save_queue;
    std::mutex              save_mutex;
    std::condition_variable save_cv;
};