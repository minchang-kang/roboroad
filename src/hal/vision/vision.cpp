#include "vision.h"
#include <atomic>
#include <iostream>

Vision::Vision(const std::string& device, const std::string& role, int width, int height, int fps)
    : device_(device), role_(role),
      width_(width), height_(height), target_fps_(fps) {}

Vision::~Vision() { release(); }

bool Vision::open() {
    cap_.open(device_, cv::CAP_V4L2);
    if (!cap_.isOpened()) {
        std::cerr << "[Vision:" << role_ << "] 카메라를 열 수 없습니다. device=" << device_ << std::endl;
        return false;
    }

    cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    cap_.set(cv::CAP_PROP_FRAME_WIDTH,  width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap_.set(cv::CAP_PROP_FPS,          target_fps_);

    int fourcc = static_cast<int>(cap_.get(cv::CAP_PROP_FOURCC));
    char fourcc_str[5] = {0};
    fourcc_str[0] = fourcc & 0xFF;
    fourcc_str[1] = (fourcc >> 8) & 0xFF;
    fourcc_str[2] = (fourcc >> 16) & 0xFF;
    fourcc_str[3] = (fourcc >> 24) & 0xFF;

    std::cout << "[Vision:" << role_ << "] 초기화 완료" << std::endl;
    std::cout << "  코덱: " << fourcc_str << std::endl;
    std::cout << "  FPS:  " << cap_.get(cv::CAP_PROP_FPS) << std::endl;
    std::cout << "  해상도: "
              << cap_.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap_.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;

    for (int i = 0; i < 30; ++i) {
        cv::Mat tmp;
        cap_.read(tmp);
    }

    return true;
}

bool Vision::read(cv::Mat& frame) {
    auto t1 = std::chrono::steady_clock::now();
    bool ret = cap_.read(frame);
    auto t2 = std::chrono::steady_clock::now();

    static std::atomic<int> count = 0;
    if (++count % 50 == 0) {
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() / 1000.0;
        std::cout << "[" << role_ << "::read] " << ms << " ms" << std::endl;
    }
    return ret;
}

void Vision::release() {
    if (cap_.isOpened()) cap_.release();
}

bool Vision::isOpened() const {
    return cap_.isOpened();
}

double Vision::actual_fps() const {
    return cap_.get(cv::CAP_PROP_FPS);
}