#include "save/save_manager.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

SaveManager::SaveManager(const YAML::Node& config)
    : output_path_(config["output_path"].as<std::string>()) {}

SaveManager::~SaveManager() {
    stop();
}

bool SaveManager::isRecording() const {
    return is_recording_;
}

bool SaveManager::start() {
    try {
        fs::create_directories(output_path_);
        file_ = H5::H5File(generateFilename(), H5F_ACC_TRUNC);
        file_.createGroup("/observations");
        file_.createGroup("/observations/images");
        is_recording_        = true;
        datasets_initialized_ = false;
        frame_count_         = 0;
        return true;
    } catch (H5::Exception& e) {
        std::cerr << "[SaveManager] HDF5 파일 생성 실패: " << e.getCDetailMsg() << std::endl;
        return false;
    }
}

void SaveManager::initDatasets(const SaveData& first) {
    H5::DSetCreatPropList plist;
    plist.setDeflate(4);

    // ── 스칼라 datasets (timestamp, joint) ──────────────
    {
        hsize_t init[2] = {0, 1};
        hsize_t max[2]  = {H5S_UNLIMITED, 1};
        hsize_t chunk[2]= {64, 1};
        plist.setChunk(2, chunk);

        H5::DataSpace sp(2, init, max);
        ds_timestamp_    = file_.createDataSet("/timestamps",
            H5::PredType::NATIVE_INT64, sp, plist);

        hsize_t init6[2] = {0, 6};
        hsize_t max6[2]  = {H5S_UNLIMITED, 6};
        hsize_t chunk6[2]= {64, 6};
        plist.setChunk(2, chunk6);
        H5::DataSpace sp6(2, init6, max6);
        ds_master_joint_ = file_.createDataSet("/observations/master_joint",
            H5::PredType::NATIVE_DOUBLE, sp6, plist);
        ds_ur_joint_     = file_.createDataSet("/observations/ur_joint",
            H5::PredType::NATIVE_DOUBLE, sp6, plist);
    }

    // ── 이미지 datasets ──────────────────────────────────
    for (const auto& [role, fd] : first.frames) {
        if (fd.frame.empty()) continue;
        int H = fd.frame.rows, W = fd.frame.cols;

        hsize_t init[4] = {0, (hsize_t)H, (hsize_t)W, 3};
        hsize_t max[4]  = {H5S_UNLIMITED, (hsize_t)H, (hsize_t)W, 3};
        hsize_t chunk[4]= {1, (hsize_t)H, (hsize_t)W, 3};
        plist.setChunk(4, chunk);

        H5::DataSpace sp(4, init, max);
        ds_images_[role] = file_.createDataSet(
            "/observations/images/" + role,
            H5::PredType::NATIVE_UINT8, sp, plist);
    }

    datasets_initialized_ = true;
}

void SaveManager::appendRow(const SaveData& data) {
    hsize_t n = frame_count_;

    // ── timestamp ────────────────────────────────────────
    {
        hsize_t newdims[2] = {n + 1, 1};
        ds_timestamp_.extend(newdims);
        H5::DataSpace fsp = ds_timestamp_.getSpace();
        hsize_t start[2] = {n, 0}, count[2] = {1, 1};
        fsp.selectHyperslab(H5S_SELECT_SET, count, start);
        H5::DataSpace msp(2, count);
        int64_t ts = static_cast<int64_t>(data.timestamp_us);
        ds_timestamp_.write(&ts, H5::PredType::NATIVE_INT64, msp, fsp);
    }

    // ── master joint ─────────────────────────────────────
    {
        hsize_t newdims[2] = {n + 1, 6};
        ds_master_joint_.extend(newdims);
        H5::DataSpace fsp = ds_master_joint_.getSpace();
        hsize_t start[2] = {n, 0}, count[2] = {1, 6};
        fsp.selectHyperslab(H5S_SELECT_SET, count, start);
        H5::DataSpace msp(2, count);
        ds_master_joint_.write(data.master.joint_angle,
            H5::PredType::NATIVE_DOUBLE, msp, fsp);
    }

    // ── ur joint ─────────────────────────────────────────
    {
        hsize_t newdims[2] = {n + 1, 6};
        ds_ur_joint_.extend(newdims);
        H5::DataSpace fsp = ds_ur_joint_.getSpace();
        hsize_t start[2] = {n, 0}, count[2] = {1, 6};
        fsp.selectHyperslab(H5S_SELECT_SET, count, start);
        H5::DataSpace msp(2, count);
        ds_ur_joint_.write(data.ur.joint_angle,
            H5::PredType::NATIVE_DOUBLE, msp, fsp);
    }

    // ── 이미지 ───────────────────────────────────────────
    for (auto& [role, ds] : ds_images_) {
        auto it = data.frames.find(role);
        if (it == data.frames.end() || it->second.frame.empty()) continue;

        int H = it->second.frame.rows, W = it->second.frame.cols;
        hsize_t newdims[4] = {n + 1, (hsize_t)H, (hsize_t)W, 3};
        ds.extend(newdims);

        H5::DataSpace fsp = ds.getSpace();
        hsize_t start[4] = {n, 0, 0, 0};
        hsize_t count[4] = {1, (hsize_t)H, (hsize_t)W, 3};
        fsp.selectHyperslab(H5S_SELECT_SET, count, start);
        H5::DataSpace msp(4, count);

        cv::Mat rgb;
        cv::cvtColor(it->second.frame, rgb, cv::COLOR_BGR2RGB);
        ds.write(rgb.data, H5::PredType::NATIVE_UINT8, msp, fsp);
    }
}

void SaveManager::save(const SaveData& data) {
    if (!is_recording_) return;

    if (!datasets_initialized_)
        initDatasets(data);

    appendRow(data);
    frame_count_++;
}

void SaveManager::stop() {
    if (is_recording_) {
        file_.close();
        is_recording_ = false;
        std::cout << "[SaveManager] 저장 완료 | 프레임: " << frame_count_ << std::endl;
    }
}

std::string SaveManager::generateFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    char filename[64];
    std::strftime(filename, sizeof(filename), "data_%Y%m%d_%H%M%S.hdf5", tm);
    return output_path_ + std::string(filename);
}