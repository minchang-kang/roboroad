cmake_minimum_required(VERSION 3.16)
project(ur_rtde_test)

set(CMAKE_CXX_STANDARD 17)

set(PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ur_rtde ur_rtde_build)

add_executable(ur_rtde_cmake_example rtde_client.cpp)
target_link_libraries(ur_rtde_cmake_example PRIVATE rtde)