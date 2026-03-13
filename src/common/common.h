#pragma once
#include <cstdint>
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

struct VisionQueue {
    std::deque<FrameData> data;
    mutable std::mutex    mutex;
    size_t                max_size;

    explicit VisionQueue(size_t max_size_) : max_size(max_size_) {}

    void push(FrameData frame) {
        std::lock_guard<std::mutex> lock(mutex);
        if (data.size() >= max_size)
            data.pop_front();
        data.push_back(std::move(frame));
    }

    // ref_ts에 가장 가까운 프레임 반환, 그보다 오래된 프레임은 버림
    bool pop_closest(uint64_t ref_ts, FrameData& out) {
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
};


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


// ─── 저장 데이터 ──────────────────────────────────────

struct SaveData {
    MasterState                      master;
    URState                          ur;
    std::map<std::string, FrameData> frames;  // role → frame
    uint64_t                         timestamp_us;
};


// ─── 시스템 플래그 ────────────────────────────────────

enum class SystemFlag : uint8_t {
    IDLE    = 0b00000000,
    HANDLE  = 0b00000001,   // GC 모드 변경 트리거
    SPRAY   = 0b00000010,   // trigger 버튼
    TELEOP  = 0b00000100,   // 키보드 on/off
    SAVING  = 0b00001000    // 키보드 on/off
};


// ─── 비트 연산자 ─────────────────────────────────────

inline SystemFlag operator|(SystemFlag a, SystemFlag b) {
    return static_cast<SystemFlag>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SystemFlag operator&(SystemFlag a, SystemFlag b) {
    return static_cast<SystemFlag>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}


// ─── 플래그 헬퍼 함수 ────────────────────────────────

// 특정 플래그가 켜져있는지 확인
inline bool hasFlag(SystemFlag state, SystemFlag flag) {
    return (state & flag) == flag;
}

// 특정 플래그 켜기
inline SystemFlag setFlag(SystemFlag state, SystemFlag flag) {
    return state | flag;
}

// 특정 플래그 끄기
inline SystemFlag clearFlag(SystemFlag state, SystemFlag flag) {
    return static_cast<SystemFlag>(
        static_cast<uint8_t>(state) & ~static_cast<uint8_t>(flag));
}


// ─── 공유 컨텍스트 ───────────────────────────────────

struct SharedContext {
    MasterState master_state;
    URState     ur_state;
    SystemFlag  system_flag = SystemFlag::IDLE;

    mutable std::shared_mutex master_mutex;
    mutable std::shared_mutex ur_mutex;
    std::mutex                flag_mutex;

    std::map<std::string, std::unique_ptr<VisionQueue>> vision_queues;

    std::queue<SaveData>            save_queue;
    std::mutex                      save_mutex;
    std::condition_variable         save_cv;
};