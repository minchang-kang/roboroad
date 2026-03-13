#include "vision_manager.h"
#include <stdexcept>
#include <iostream>

VisionManager::VisionManager(const YAML::Node& config, SharedContext& ctx) {
    const auto& vcfg = config["vision"];
    int    width      = vcfg["width"].as<int>();
    int    height     = vcfg["height"].as<int>();
    int    fps        = vcfg["fps"].as<int>();
    size_t queue_size = vcfg["queue_size"].as<size_t>();

    for (const auto& cam : vcfg["cameras"]) {
        std::string device = cam["device"].as<std::string>();
        std::string role   = cam["role"].as<std::string>();
        cameras_.emplace(role, Vision(device, role, width, height, fps));
        ctx.vision_queues[role] = std::make_unique<VisionQueue>(queue_size);
    }
}

bool VisionManager::openAll() {
    bool ok = true;
    for (auto& [role, cam] : cameras_) {
        if (!cam.open()) {
            std::cerr << "[VisionManager] " << role << " 열기 실패" << std::endl;
            ok = false;
        }
    }
    return ok;
}

void VisionManager::releaseAll() {
    for (auto& [role, cam] : cameras_)
        cam.release();
}

Vision& VisionManager::get(const std::string& role) {
    auto it = cameras_.find(role);
    if (it == cameras_.end())
        throw std::runtime_error("[VisionManager] 없는 role: " + role);
    return it->second;
}

bool VisionManager::has(const std::string& role) const {
    return cameras_.count(role) > 0;
}