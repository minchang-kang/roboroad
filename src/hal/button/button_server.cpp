#include "button_server.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

ButtonServer::ButtonServer(const YAML::Node& config) {
    port_ = config["button"]["port"].as<int>(5000);
}

ButtonServer::~ButtonServer() {
    close();
}

bool ButtonServer::init() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[ButtonServer] socket() 실패" << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[ButtonServer] bind() 실패 (port=" << port_ << ")" << std::endl;
        return false;
    }

    if (listen(server_fd_, 1) < 0) {
        std::cerr << "[ButtonServer] listen() 실패" << std::endl;
        return false;
    }

    std::cout << "[ButtonServer] 포트 " << port_ << " 대기 중..." << std::endl;
    return true;
}

// select()로 fd가 준비될 때까지 최대 timeout_ms ms 대기
// 준비되면 true, 타임아웃이면 false, 에러면 false
static bool wait_readable(int fd, int timeout_ms) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(fd + 1, &fds, nullptr, nullptr, &tv) > 0;
}

void ButtonServer::run(SharedContext& ctx, const std::atomic<bool>& running) {
    while (running) {
        // accept() 전에 select()로 100ms 타임아웃 → running 주기적 체크
        if (!wait_readable(server_fd_, 100))
            continue;

        client_fd_ = accept(server_fd_, nullptr, nullptr);
        if (client_fd_ < 0) {
            if (!running) break;
            std::cerr << "[ButtonServer] accept() 실패" << std::endl;
            continue;
        }
        std::cout << "[ButtonServer] 라즈베리파이 연결됨" << std::endl;

        std::string buf;
        buf.reserve(16);
        char c;
        while (running) {
            // recv() 전에 select()로 100ms 타임아웃 → running 주기적 체크
            if (!wait_readable(client_fd_, 100))
                continue;

            ssize_t n = recv(client_fd_, &c, 1, 0);
            if (n <= 0) {
                std::cout << "[ButtonServer] 연결 끊김, 재대기..." << std::endl;
                break;
            }
            if (c == '\n') {
                std::lock_guard<std::mutex> lock(ctx.flag_mutex);
                if (buf == "ON") {
                    ctx.system_flag = setFlag(ctx.system_flag, SystemFlag::SPRAY);
                    std::cout << "[ButtonServer] SPRAY ON" << std::endl;
                } else if (buf == "OFF") {
                    ctx.system_flag = clearFlag(ctx.system_flag, SystemFlag::SPRAY);
                    std::cout << "[ButtonServer] SPRAY OFF" << std::endl;
                }
                buf.clear();
            } else {
                buf += c;
            }
        }

        ::close(client_fd_);
        client_fd_ = -1;
    }
}

void ButtonServer::close() {
    if (client_fd_ >= 0) { ::close(client_fd_); client_fd_ = -1; }
    if (server_fd_ >= 0) { ::close(server_fd_); server_fd_ = -1; }
}
