``` cpp
#include <yaml-cpp/yaml.h>

YAML::Node config = YAML::LoadFile("config.yaml");

int gc_loop_hz = config["gc_loop_hz"].as<int>();
std::string ur_ip = config["ur_ip"].as<std::string>();
double threshold = config["fsr_pressure_threshold"].as<double>();

```