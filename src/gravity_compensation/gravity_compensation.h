#pragma once
#include "common/common.h"
#include <string>

class GravityCompensation {
public:
    GravityCompensation(const std::string& urdf_path);
    ~GravityCompensation();

    bool init();
    void update(const MasterState& master, const FSRState& fsr);

private:
    std::string urdf_path_;

    // Robotics Library 관련 객체들
    // 동역학 계산 모델
};