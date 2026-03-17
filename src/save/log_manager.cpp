// ============================================================================
// log_manager.cpp
//
// 50Hz CSV 연속 로그 구현
//
// 레퍼런스: /home/mugun/Desktop/roboroad/src/log_thread.cpp
//   파일명 생성, 헤더 출력, 50Hz 데이터 행 기록 방식을 클래스로 이식
// ============================================================================

#include "save/log_manager.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// ============================================================================
// 생성자
// ============================================================================

LogManager::LogManager(const YAML::Node& config)
    : log_dir_(config["log_dir"].as<std::string>("log"))
{}

LogManager::~LogManager()
{
    close();
}

// ============================================================================
// open — 디렉터리 생성 + 타임스탬프 파일명으로 CSV 열기
// ============================================================================

bool LogManager::open()
{
    try {
        fs::create_directories(log_dir_);
    } catch (const std::exception& e) {
        std::cerr << "[LogManager] 디렉터리 생성 실패: " << e.what() << std::endl;
        return false;
    }

    std::string path = generateFilename();
    fp_ = fopen(path.c_str(), "w");
    if (!fp_) {
        std::cerr << "[LogManager] 파일 열기 실패: " << path << std::endl;
        return false;
    }

    writeHeader();
    std::cout << "[LogManager] 로그 시작: " << path << std::endl;
    return true;
}

// ============================================================================
// write — 한 행 기록 (50Hz 루프에서 호출)
//
// 컬럼 순서 (레퍼런스 log_thread.cpp 기준, 타겟 프로젝트 데이터 구조에 맞춤):
//   log_ts_ns | dxl_q[1~6]_rad | gc_tau[1~6]_nm | ur_q[1~6]_rad
// ============================================================================

void LogManager::write(const MasterState& master, const URState& ur)
{
    if (!fp_)
        return;

    // 현재 타임스탬프 [ns]
    uint64_t log_ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    fprintf(fp_,
        // log_ts_ns
        "%lu,"
        // dxl_q[1~6]_rad — 마스터(DXL) 관절각 (중립 = 0 기준)
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
        // gc_tau[1~6]_nm — GC 계산 토크 [Nm]
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
        // ur_q[1~6]_rad — UR 실제 관절각
        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",

        (unsigned long)log_ts,

        master.joint_angle[0], master.joint_angle[1], master.joint_angle[2],
        master.joint_angle[3], master.joint_angle[4], master.joint_angle[5],

        master.torque[0], master.torque[1], master.torque[2],
        master.torque[3], master.torque[4], master.torque[5],

        ur.joint_angle[0], ur.joint_angle[1], ur.joint_angle[2],
        ur.joint_angle[3], ur.joint_angle[4], ur.joint_angle[5]
    );
}

// ============================================================================
// close
// ============================================================================

void LogManager::close()
{
    if (fp_) {
        fclose(fp_);
        fp_ = nullptr;
        std::cout << "[LogManager] 로그 종료" << std::endl;
    }
}

// ============================================================================
// generateFilename — YYYYMMDD_HHMMSS_integrated.csv
//   레퍼런스 log_thread.cpp 의 파일명 형식과 동일
// ============================================================================

std::string LogManager::generateFilename()
{
    time_t t = time(nullptr);
    struct tm tm_info;
    localtime_r(&t, &tm_info);

    char buf[512];
    snprintf(buf, sizeof(buf),
             "%s/%04d%02d%02d_%02d%02d%02d_integrated.csv",
             log_dir_.c_str(),
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);

    return std::string(buf);
}

// ============================================================================
// writeHeader — CSV 컬럼 헤더 출력
// ============================================================================

void LogManager::writeHeader()
{
    fprintf(fp_,
        "log_ts_ns,"
        "dxl_q1_rad,dxl_q2_rad,dxl_q3_rad,dxl_q4_rad,dxl_q5_rad,dxl_q6_rad,"
        "gc_tau1_nm,gc_tau2_nm,gc_tau3_nm,gc_tau4_nm,gc_tau5_nm,gc_tau6_nm,"
        "ur_q1_rad,ur_q2_rad,ur_q3_rad,ur_q4_rad,ur_q5_rad,ur_q6_rad\n");
    fflush(fp_);
}
