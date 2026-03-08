#include "hal/ur/ur_hal.h"

URHal::URHal(const std::string& ip) : ip_(ip) {}

URHal::~URHal() {}

bool URHal::init() { return true; }

void URHal::close() {}

bool URHal::readJointAngles(URState& state) { return true; }

bool URHal::writeJointAngles(const URState& state) { return true; }