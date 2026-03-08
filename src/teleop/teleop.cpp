#include "teleop/teleop.h"

Teleop::Teleop(URHal& ur) : ur_(ur) {}

Teleop::~Teleop() {}

void Teleop::update(const MasterState& master, const SystemFlag& flag) {}

bool Teleop::moveToHome() { return true; }