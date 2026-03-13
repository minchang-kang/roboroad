#pragma once
#include <string>
#include "common/common.h"
#include <opencv2/opencv.hpp>

class Vision {
public:
    Vision(const std::string& device, const std::string& role, int width, int height, int fps);
    ~Vision();

    bool open();
    bool read(cv::Mat& frame);
    void release();
    bool isOpened() const;

    const std::string& role() const { return role_; }
    int width() const { return width_; }
    int height() const { return height_; }
    double actual_fps() const;

private:
    cv::VideoCapture cap_;
    std::string device_;
    std::string role_;
    int width_;
    int height_;
    int target_fps_;
};