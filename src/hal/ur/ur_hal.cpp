// ============================================================================
// ur_hal.cpp
//
// UR 로봇 RTDE 통신 구현
//
// 레퍼런스: /home/mugun/Desktop/roboroad/src/ur_threads.cpp
//   - ur_rtde_thread  : RTDEReceiveInterface → readJointAngles()
//   - ur_servo_thread : RTDEControlInterface servoJ → writeJointAngles()
// ============================================================================

#include "hal/ur/ur_hal.h"

#include <ur_rtde/rtde_receive_interface.h>
#include <ur_rtde/rtde_control_interface.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>

// ============================================================================
// 생성자
// ============================================================================

URHal::URHal(const YAML::Node& config)
    : ip_(config["ur"]["ip"].as<std::string>())
    , servoj_dt_(0.002)              // 500Hz 주기
    , servoj_lookahead_time_(0.05)
    , servoj_gain_(700.0)
{
    // 홈 위치 기본값
    home_q_[0] =  0.0;
    home_q_[1] = -M_PI / 2.0;
    home_q_[2] =  0.0;
    home_q_[3] = -M_PI / 2.0;
    home_q_[4] =  0.0;
    home_q_[5] =  0.0;

    const auto& uc = config["ur"];

    if (uc["servoj_lookahead_time"])
        servoj_lookahead_time_ = uc["servoj_lookahead_time"].as<double>();

    if (uc["servoj_gain"])
        servoj_gain_ = uc["servoj_gain"].as<double>();

    if (uc["home_deg"] && uc["home_deg"].IsSequence()) {
        int n = std::min(6, (int)uc["home_deg"].size());
        for (int i = 0; i < n; i++)
            home_q_[i] = uc["home_deg"][i].as<double>() * M_PI / 180.0;
    }
}

URHal::~URHal()
{
    close();
}

// ============================================================================
// init — RTDEReceiveInterface / RTDEControlInterface 연결
// ============================================================================

bool URHal::init()
{
    try {
        rtde_receive_ = std::make_unique<ur_rtde::RTDEReceiveInterface>(ip_);
        rtde_control_ = std::make_unique<ur_rtde::RTDEControlInterface>(ip_);

        int64_t ur_us  = static_cast<int64_t>(rtde_receive_->getTimestamp() * 1e6);
        int64_t sys_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        timestamp_offset_us_ = sys_us - ur_us;

        std::cout << "[URHal] Connected to UR at " << ip_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[URHal] init failed: " << e.what() << std::endl;
        rtde_control_.reset();
        rtde_receive_.reset();
        return false;
    }
}

// ============================================================================
// close — servoJ 중단 후 연결 해제
// ============================================================================

void URHal::close()
{
    if (rtde_control_) {
        try {
            rtde_control_->servoStop();
            rtde_control_->stopScript();
        } catch (...) {}
        rtde_control_.reset();
    }
    if (rtde_receive_) {
        rtde_receive_.reset();
    }
    std::cout << "[URHal] Connection closed" << std::endl;
}

// ============================================================================
// readJointAngles — RTDEReceiveInterface::getActualQ()
//   레퍼런스 ur_rtde_thread 에서 getActualQ() 로 수신하던 부분
// ============================================================================

bool URHal::readJointAngles(URState& state)
{
    if (!rtde_receive_)
        return false;

    try {
        std::vector<double> q = rtde_receive_->getActualQ();
        if ((int)q.size() < 6)
            return false;

        for (int i = 0; i < 6; i++)
            state.joint_angle[i] = q[i];

        state.timestamp_us = static_cast<int64_t>(
            rtde_receive_->getTimestamp() * 1e6) + timestamp_offset_us_;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[URHal] readJointAngles: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// writeJointAngles — RTDEControlInterface::servoJ()
//   레퍼런스 ur_servo_thread 의 servoJ(cmd_q, 0.0, 0.0, 0.002, 0.05, 700.0)
// ============================================================================

bool URHal::writeJointAngles(const URState& state)
{
    if (!rtde_control_)
        return false;

    try {
        std::vector<double> q(state.joint_angle, state.joint_angle + 6);
        return rtde_control_->servoJ(q,
                                     0.0,                    // speed    (servoJ 에서 미사용)
                                     0.0,                    // accel    (servoJ 에서 미사용)
                                     servoj_dt_,             // dt = 0.002 (500Hz)
                                     servoj_lookahead_time_, // 0.05 s
                                     servoj_gain_);          // 700
    } catch (const std::exception& e) {
        std::cerr << "[URHal] writeJointAngles: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// moveToHome — servoJ 중단 후 moveJ 로 홈 위치 이동
// ============================================================================

bool URHal::moveToHome()
{
    if (!rtde_control_)
        return false;

    try {
        rtde_control_->servoStop();

        std::vector<double> q(home_q_, home_q_ + 6);
        // speed=0.5 rad/s, accel=0.5 rad/s² — 안전한 저속 이동
        return rtde_control_->moveJ(q, 0.5, 0.5);
    } catch (const std::exception& e) {
        std::cerr << "[URHal] moveToHome: " << e.what() << std::endl;
        return false;
    }
}
