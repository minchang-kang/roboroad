#pragma once
#include <map>
#include <string>
#include <mutex>
#include <iostream>

// 디버깅/검증용 진단 컨텍스트 (SharedContext와 분리)
struct DiagContext {
    mutable std::mutex mtx;
    std::map<std::string, uint64_t> vision_drop;  // role → SAVING 중 드롭 수

    void record_drop(const std::string& role) {
        std::lock_guard<std::mutex> lock(mtx);
        ++vision_drop[role];
    }

    // 세션 종료 시 경고 출력
    void report() const {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& [role, cnt] : vision_drop)
            if (cnt > 0)
                std::cerr << "[Diag] 경고: [" << role << "] "
                          << cnt << "개 프레임 드롭 (저장 세션 중)\n";
    }

    // 다음 세션을 위해 카운터 초기화
    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        vision_drop.clear();
    }
};
