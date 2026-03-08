#include "hal/dynamixel/dynamixel_hal.h"

DynamixelHAL::DynamixelHAL(const YAML::Node& config) {}

DynamixelHAL::~DynamixelHAL() {}

bool DynamixelHAL::init() { return true; }

void DynamixelHAL::close() {}

bool DynamixelHAL::readAngles(MasterState& state) { return true; }

bool DynamixelHAL::writeTorque(const MasterState& state) { return true; }

bool DynamixelHAL::setTorqueEnable(bool enable) { return true; }
