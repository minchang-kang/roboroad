#pragma once
#include <atomic>
#include <yaml-cpp/yaml.h>
#include "../../common/common.h"

class ButtonServer {
public:
    explicit ButtonServer(const YAML::Node& config);
    ~ButtonServer();

    bool init();
    void run(SharedContext& ctx, const std::atomic<bool>& running);
    void close();

private:
    int port_;
    int server_fd_ = -1;
    int client_fd_ = -1;
};
