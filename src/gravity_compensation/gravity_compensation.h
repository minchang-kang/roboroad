#pragma once
#include "common/common.h"
#include <string>

class GravityCompensation {
public:
    GravityCompensation(const YAML::Node& config);
    ~GravityCompensation();

    bool init();
    void update(const MasterState& master);
    // 토크 계산 함수, 

private:
    std::string urdf_path_;

    // Robotics Library 관련 객체들
    // 동역학 계산 모델
};