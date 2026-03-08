#pragma once
#include "common/common.h"
#include "hal/ur/ur_hal.h"

class Teleop {
public:
    Teleop(URHal& ur);
    ~Teleop();

    bool init();
    void update(const MasterState& master, const SystemFlag& flag);
    bool moveToHome();

private:
    URHal& ur_;
};