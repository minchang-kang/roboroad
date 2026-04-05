#include "hal/mouse/mouse_handler.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/input.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

static constexpr uint8_t  PROTOCOL_VERSION = 2;
static constexpr uint16_t ADDR_RETURN_DELAY = 9;
static constexpr uint16_t ADDR_OP_MODE      = 11;
static constexpr uint16_t ADDR_TORQUE_EN    = 64;
static constexpr uint16_t ADDR_GOAL_CUR     = 102;
static constexpr uint8_t  MODE_CURRENT_CTRL = 0;

// ============================================================================
// 생성자
// ============================================================================

MouseHandler::MouseHandler(const YAML::Node& config)
    : device_name_ (config["mouse"]["device_name"].as<std::string>())
    , port_        (config["mouse"]["port"].as<std::string>())
    , baudrate_    (config["mouse"]["baudrate"].as<int>(4000000))
    , motor_id_    (config["mouse"]["motor_id"].as<uint8_t>(10))
    , goal_current_(config["mouse"]["goal_current"].as<int16_t>(200))
{}

// ============================================================================
// findDevicePath — /proc/bus/input/devices에서 장치명으로 eventN 경로 탐색
// ============================================================================

std::string MouseHandler::findDevicePath(const std::string& device_name)
{
    std::ifstream f("/proc/bus/input/devices");
    if (!f) throw std::runtime_error("Cannot open /proc/bus/input/devices");

    std::string line, block, event_token;
    bool name_matched = false;

    auto parse_block = [&]() {
        if (!name_matched) return;
        std::istringstream ss(block);
        std::string l;
        while (std::getline(ss, l)) {
            if (l.rfind("H:", 0) == 0) {
                std::smatch m;
                if (std::regex_search(l, m, std::regex(R"(event\d+)")))
                    event_token = m[0];
            }
        }
    };

    while (std::getline(f, line)) {
        if (line.empty()) {
            parse_block();
            block.clear();
            name_matched = false;
            continue;
        }
        if (line.rfind("N:", 0) == 0 && line.find(device_name) != std::string::npos)
            name_matched = true;
        block += line + '\n';
    }
    parse_block();

    if (event_token.empty())
        throw std::runtime_error("[MouseHandler] 장치를 찾을 수 없음: " + device_name);

    return "/dev/input/" + event_token;
}

// ============================================================================
// init — 마우스 장치 열기 + 모터 10 전용 포트 초기화
// ============================================================================

bool MouseHandler::init()
{
    // ── 마우스 장치 열기 ────────────────────────────────────────────────────
    std::string path;
    try {
        path = findDevicePath(device_name_);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
    std::cout << "[MouseHandler] 마우스 장치: " << path << std::endl;

    mouse_fd_ = open(path.c_str(), O_RDONLY);
    if (mouse_fd_ < 0) {
        std::cerr << "[MouseHandler] 장치 열기 실패: " << path << std::endl;
        return false;
    }
    ioctl(mouse_fd_, EVIOCGRAB, reinterpret_cast<void*>(1));

    // ── 모터 10 전용 포트 열기 ──────────────────────────────────────────────
    port_handler_   = dynamixel::PortHandler::getPortHandler(port_.c_str());
    packet_handler_ = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);

    if (!port_handler_->openPort()) {
        std::cerr << "[MouseHandler] 포트 열기 실패: " << port_ << std::endl;
        return false;
    }
    if (!port_handler_->setBaudRate(baudrate_)) {
        std::cerr << "[MouseHandler] 보드레이트 설정 실패: " << baudrate_ << std::endl;
        return false;
    }
    std::cout << "[MouseHandler] 포트 오픈: " << port_ << " @ " << baudrate_ << " bps" << std::endl;

    // ── 모터 10 초기화 (토크 OFF → 전류 제어 모드 → 토크 ON) ───────────────
    uint8_t err = 0;
    auto w1 = [&](uint16_t addr, uint8_t val) {
        return packet_handler_->write1ByteTxRx(
            port_handler_, motor_id_, addr, val, &err) == COMM_SUCCESS;
    };

    if (!w1(ADDR_TORQUE_EN, 0)       ||
        !w1(ADDR_RETURN_DELAY, 0)    ||
        !w1(ADDR_OP_MODE, MODE_CURRENT_CTRL) ||
        !w1(ADDR_TORQUE_EN, 1)) {
        std::cerr << "[MouseHandler] 모터 ID " << (int)motor_id_ << " 초기화 실패" << std::endl;
        return false;
    }

    std::cout << "[MouseHandler] 초기화 완료 (motor_id=" << (int)motor_id_
              << ", goal_current=" << goal_current_ << ")" << std::endl;
    return true;
}

// ============================================================================
// writeCurrent — 모터 10에 전류 직접 명령
// ============================================================================

bool MouseHandler::writeCurrent(int16_t current)
{
    uint8_t err = 0;
    return packet_handler_->write2ByteTxRx(
        port_handler_, motor_id_, ADDR_GOAL_CUR,
        static_cast<uint16_t>(current), &err) == COMM_SUCCESS;
}

// ============================================================================
// run — 좌클릭 press → SPRAY ON + 모터 구동 / release → SPRAY OFF + 모터 정지
// ============================================================================

void MouseHandler::run(SharedContext& ctx, const std::atomic<bool>& running)
{
    struct input_event ev;

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(mouse_fd_, &rfds);

        struct timeval tv{0, 100'000};
        int ret = select(mouse_fd_ + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) break;
            continue;
        }
        if (ret == 0) continue;

        ssize_t n = read(mouse_fd_, &ev, sizeof(ev));
        if (n < static_cast<ssize_t>(sizeof(ev))) continue;

        if (ev.type != EV_KEY || ev.code != BTN_LEFT) continue;

        uint64_t ts_us = static_cast<uint64_t>(ev.time.tv_sec) * 1'000'000ULL
                       + static_cast<uint64_t>(ev.time.tv_usec);

        if (ev.value == 1) {
            ctx.spray_on.store(true);
            {
                std::lock_guard<std::mutex> lock(ctx.mouse_mutex);
                ctx.mouse_state = {1, ts_us};
            }
            writeCurrent(goal_current_);
            std::cout << "[MouseHandler] SPRAY ON  (motor=" << (int)motor_id_
                      << " cur=" << goal_current_ << ")" << std::endl;
        } else if (ev.value == 0) {
            ctx.spray_on.store(false);
            {
                std::lock_guard<std::mutex> lock(ctx.mouse_mutex);
                ctx.mouse_state = {0, ts_us};
            }
            writeCurrent(0);
            std::cout << "[MouseHandler] SPRAY OFF (motor=" << (int)motor_id_ << ")" << std::endl;
        }
    }
}

// ============================================================================
// close — 모터 정지 + 포트 닫기 + 마우스 장치 해제
// ============================================================================

void MouseHandler::close()
{
    if (packet_handler_ && port_handler_) {
        writeCurrent(0);
        uint8_t err = 0;
        packet_handler_->write1ByteTxRx(
            port_handler_, motor_id_, ADDR_TORQUE_EN, 0, &err);
        port_handler_->closePort();
    }
    port_handler_   = nullptr;
    packet_handler_ = nullptr;

    if (mouse_fd_ >= 0) {
        ioctl(mouse_fd_, EVIOCGRAB, reinterpret_cast<void*>(0));
        ::close(mouse_fd_);
        mouse_fd_ = -1;
    }
}
