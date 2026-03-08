#pragma once
#include <cstdint>
#include <mutex>
#include <yaml-cpp/yaml.h>

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
    URState ur_state;
    SystemFlag system_flag = SystemFlag::IDLE;

    std::mutex master_mutex;
    std::mutex ur_mutex;
    std::mutex fsr_mutex;
    std::mutex flag_mutex;
};