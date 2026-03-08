#pragma once
#include "common/common.h"
#include <string>

class URHal {
public:
    URHal(const YAML::Node& config);
    ~URHal();

    bool init();
    void close();

    bool readJointAngles(URState& state);
    bool writeJointAngles(const URState& state);

private:
    std::string ip_;

    // ur_rtde 관련 객체들
    // RTDEReceiveInterface, RTDEControlInterface
};