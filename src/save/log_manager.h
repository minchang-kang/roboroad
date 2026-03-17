#pragma once
#include "common/common.h"
#include <cstdio>
#include <string>

// ============================================================================
// LogManager — 50Hz CSV 연속 로그
//
// SaveManager(HDF5) 와 달리 SAVING 플래그와 무관하게 항상 기록한다.
// 실행 시작부터 종료까지 전체 구간을 남겨 분석/디버깅에 사용한다.
//
// 출력 경로: config.yaml log_dir / YYYYMMDD_HHMMSS_integrated.csv
//
// CSV 컬럼:
//   log_ts_ns                       — 로그 타임스탬프 [ns]
//   dxl_q1_rad ~ dxl_q6_rad        — 마스터(DXL) 관절각 [rad]
//   gc_tau1_nm  ~ gc_tau6_nm        — GC 계산 토크 [Nm]
//   ur_q1_rad   ~ ur_q6_rad         — UR 실제 관절각 [rad]
//
// 레퍼런스: /home/mugun/Desktop/roboroad/src/log_thread.cpp
// ============================================================================

class LogManager {
public:
    explicit LogManager(const YAML::Node& config);
    ~LogManager();

    // 타임스탬프 파일명으로 CSV 열기
    bool open();

    // 한 행 기록 — 50Hz 루프에서 호출
    void write(const MasterState& master, const URState& ur);

    // 파일 닫기
    void close();

    bool isOpen() const { return fp_ != nullptr; }

private:
    std::string log_dir_;
    FILE*       fp_ = nullptr;

    std::string generateFilename();
    void        writeHeader();
};
