#include "hal/dynamixel/dynamixel_hal.h"

DynamixelHAL::DynamixelHAL(const std::string& port, int baudrate)
    : port_(port), baudrate_(baudrate) {}

DynamixelHAL::~DynamixelHAL() {}

bool DynamixelHAL::init() { return true; }

void DynamixelHAL::close() {}

bool DynamixelHAL::readAngles(MasterState& state) { return true; }

bool DynamixelHAL::writeTorque(const MasterState& state) { return true; }

bool DynamixelHAL::setTorqueEnable(bool enable) { return true; }
