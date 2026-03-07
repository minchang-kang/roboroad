#include <ur_rtde/rtde_receive_interface.h>
#include <vector>
#include <iostream>
#include <csignal>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <climits>

using namespace ur_rtde;

static volatile bool running = true;
void sigHandler(int) { running = false; }

static inline long long toNs(const timespec& ts) {
    return (long long)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
}

static inline timespec fromNs(long long ns) {
    timespec ts;
    ts.tv_sec  = ns / 1'000'000'000LL;
    ts.tv_nsec = ns % 1'000'000'000LL;
    return ts;
}

int main() {
    const std::string robot_ip        = "192.168.84.100";
    const double      rtde_frequency  = 500.0;
    const int         rt_priority     = 90;
    const long long   period_ns       = 20'000'000LL; // 50Hz = 20ms
    const int         print_interval  = 50;           // 매 50틱마다 출력

    signal(SIGINT, sigHandler);

    RTDEReceiveInterface rtde_receive(robot_ip, rtde_frequency, {}, true, false, rt_priority);

    // 통계
    long long jitter_max_ns  = LLONG_MIN;
    long long jitter_min_ns  = LLONG_MAX;
    long long jitter_sum_ns  = 0;
    int       overrun_count  = 0;
    int       tick           = 0;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long next_ns = toNs(now);

    std::cout << "[50Hz RT loop 시작] Ctrl+C로 종료\n\n";

    while (running) {
        // 다음 주기까지 절대시간 sleep
        next_ns += period_ns;
        timespec next_ts = fromNs(next_ns);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_ts, nullptr);

        // 실제 wake-up 시간 측정
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long jitter_ns = toNs(now) - next_ns;

        // 통계 갱신
        jitter_max_ns  = std::max(jitter_max_ns, jitter_ns);
        jitter_min_ns  = std::min(jitter_min_ns, jitter_ns);
        jitter_sum_ns += jitter_ns;
        if (jitter_ns > period_ns) overrun_count++;
        tick++;

        // 각도 읽기
        std::vector<double> q = rtde_receive.getActualQ();

        // 주기적 출력
        if (tick % print_interval == 0) {
            std::cout << "tick=" << tick
                      << "  jitter=" << jitter_ns / 1000 << "us"
                      << "  max=" << jitter_max_ns / 1000 << "us"
                      << "  min=" << jitter_min_ns / 1000 << "us"
                      << "  overrun=" << overrun_count
                      << "  q[0]=" << q[0] << "rad\n";
        }
    }

    // 최종 통계
    std::cout << "\n=== 결과 ===\n"
              << "총 틱:     " << tick << "\n"
              << "지터 평균: " << jitter_sum_ns / tick / 1000 << " us\n"
              << "지터 최대: " << jitter_max_ns / 1000 << " us\n"
              << "지터 최소: " << jitter_min_ns / 1000 << " us\n"
              << "오버런:    " << overrun_count << " 회\n";

    return 0;
}
