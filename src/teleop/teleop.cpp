#include "teleop/teleop.h"

Teleop::Teleop(URHal& ur) : ur_(ur) {}

Teleop::~Teleop() {}

void Teleop::update(const MasterState& master, const SystemFlag& flag) {}

void Teleop::reset() {
    ur_.moveToHome();
}
