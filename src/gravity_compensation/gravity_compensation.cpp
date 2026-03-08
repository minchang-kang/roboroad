#include "gravity_compensation/gravity_compensation.h"

GravityCompensation::GravityCompensation(const std::string& urdf_path)
    : urdf_path_(urdf_path) {}

GravityCompensation::~GravityCompensation() {}

bool GravityCompensation::init() { return true; }

void GravityCompensation::update(const MasterState& master, const FSRState& fsr) {}