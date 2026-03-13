#pragma once
#include "common/common.h"
#include <H5Cpp.h>
#include <string>
#include <map>

class SaveManager {
public:
    explicit SaveManager(const YAML::Node& config);
    ~SaveManager();

    bool start();
    void save(const SaveData& data);
    void stop();
    bool isRecording() const;

private:
    H5::H5File file_;
    bool       is_recording_       = false;
    bool       datasets_initialized_ = false;
    hsize_t    frame_count_        = 0;

    H5::DataSet                          ds_timestamp_;
    H5::DataSet                          ds_master_joint_;
    H5::DataSet                          ds_ur_joint_;
    std::map<std::string, H5::DataSet>   ds_images_;

    void initDatasets(const SaveData& first);
    void appendRow(const SaveData& data);

    std::string        output_path_;
    std::string generateFilename();
};
