#include "recorder/recorder.h"

Recorder::Recorder() : is_recording_(false) {}

Recorder::~Recorder() {
    stop();
}

bool Recorder::isRecording() {
    return is_recording_;
}

bool Recorder::start() {
    file_.open(generateFilename());
    file_ << "timestamp_master,master_j1,master_j2,master_j3,master_j4,master_j5,master_j6,"
          << "timestamp_ur,ur_j1,ur_j2,ur_j3,ur_j4,ur_j5,ur_j6\n";
    is_recording_ = true;
    return file_.is_open();
}

void Recorder::save(const MasterState& master, const URState& ur) {
    if (!is_recording_) return;
    file_ << master.timestamp_us << ","
          << master.joint_angle[0] << "," << master.joint_angle[1] << ","
          << master.joint_angle[2] << "," << master.joint_angle[3] << ","
          << master.joint_angle[4] << "," << master.joint_angle[5] << ","
          << ur.timestamp_us << ","
          << ur.joint_angle[0] << "," << ur.joint_angle[1] << ","
          << ur.joint_angle[2] << "," << ur.joint_angle[3] << ","
          << ur.joint_angle[4] << "," << ur.joint_angle[5] << "\n";
}

void Recorder::stop() {
    if (is_recording_) {
        file_.close();
        is_recording_ = false;
    }
}

std::string Recorder::generateFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&t);
    char filename[32];
    std::strftime(filename, sizeof(filename), "data_%Y%m%d_%H%M%S.csv", tm);
    return std::string(filename);
}