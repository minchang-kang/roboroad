#pragma once
#include "vision.h"
#include <map>
#include <string>
#include "common/common.h"

class VisionManager {
public:
    VisionManager(const YAML::Node& config, SharedContext& ctx);

    bool openAll();
    void releaseAll();

    Vision& get(const std::string& role);
    bool has(const std::string& role) const;

private:
    std::map<std::string, Vision> cameras_;
};
