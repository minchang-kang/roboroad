#pragma once
#include "common/common.h"
#include <fstream>
#include <string>

class Recorder {
public:
    Recorder();
    ~Recorder();

    bool start();
    void save(const MasterState& master, const URState& ur);
    void stop();
    bool isRecording();
    
private:
    std::ofstream file_;
    bool is_recording_;
    
    std::string generateFilename();
};