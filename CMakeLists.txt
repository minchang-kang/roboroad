cmake_minimum_required(VERSION 3.16)
project(roboroad)

set(CMAKE_CXX_STANDARD 17)


# ─── Xenomai (Alchemy API) ──────────────────────────
execute_process(
    COMMAND xeno-config --skin=alchemy --cflags
    OUTPUT_VARIABLE XENOMAI_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND xeno-config --skin=alchemy --ldflags
    OUTPUT_VARIABLE XENOMAI_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(XENOMAI_CFLAGS_LIST UNIX_COMMAND "${XENOMAI_CFLAGS}")
separate_arguments(XENOMAI_LDFLAGS_LIST UNIX_COMMAND "${XENOMAI_LDFLAGS}")


# ─── 3rdparty ────────────────────────────────────────
find_package(ur_rtde QUIET)
if(NOT ur_rtde_FOUND)
    message(STATUS "ur_rtde not found in system, building from submodule")
    set(EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
    add_subdirectory(3rdparty/ur_rtde)
endif()
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
    message(STATUS "yaml-cpp not found in system, building from submodule")
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    add_subdirectory(3rdparty/yaml-cpp)
endif()
find_package(RL 0.7.0 COMPONENTS MATH MDL XML QUIET)
if(NOT RL_FOUND)
    message(STATUS "RL not found in system, building from submodule")
    set(RL_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_HAL OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_SG OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_PLAN OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_VIS OFF CACHE BOOL "" FORCE)
    set(RL_BUILD_KIN OFF CACHE BOOL "" FORCE)
    add_subdirectory(3rdparty/rl)
    add_library(rl::math ALIAS math)
    add_library(rl::mdl ALIAS mdl)
    add_library(rl::xml ALIAS xml)
endif()

find_package(OpenCV REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS CXX)

# ─── DynamixelSDK ────────────────────────────────────
set(DYNAMIXEL_SRC
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_bulk_read.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_bulk_write.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_sync_read.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_sync_write.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_fast_bulk_read.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_fast_sync_read.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_handler.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/packet_handler.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/port_handler.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/port_handler_linux.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/protocol1_packet_handler.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/protocol2_packet_handler.cpp
)


# ─── 소스 파일 ──────────────────────────────────────
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/hal/dynamixel/dynamixel_hal.cpp
    src/hal/ur/ur_hal.cpp
    src/hal/vision/vision.cpp
    src/hal/vision/vision_manager.cpp
    src/gravity_compensation/gravity_compensation.cpp
    src/teleop/teleop.cpp
    src/save/save_manager.cpp
    src/save/log_manager.cpp
    src/rt/rt_task.cpp
    src/hal/mouse/mouse_handler.cpp
    ${DYNAMIXEL_SRC}
)


# ─── include 경로 ────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    src/
    3rdparty/DynamixelSDK/c++/include
    3rdparty/DynamixelSDK/c++/include/dynamixel_sdk
    /usr/local/include/rl-0.7.0
    /usr/include/eigen3
    /usr/xenomai/include
    /usr/xenomai/include/cobalt
    /usr/xenomai/include/alchemy
    ${OpenCV_INCLUDE_DIRS}
    ${HDF5_INCLUDE_DIRS}
)


# ─── 컴파일 정의 ─────────────────────────────────────
target_compile_options(${PROJECT_NAME} PRIVATE ${XENOMAI_CFLAGS_LIST})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LINUX
    _GNU_SOURCE
    CONTROL_TABLE_PATH="/usr/local/share/dynamixel_sdk/control_table"
)


# ─── 링크 ────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME}
    ur_rtde::rtde
    yaml-cpp
    rl::mdl
    rl::math
    rl::xml
    pthread
    rt
    ${OpenCV_LIBS}
    ${HDF5_CXX_LIBRARIES}
    ${XENOMAI_LDFLAGS_LIST}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "/usr/xenomai/lib"
    INSTALL_RPATH "/usr/xenomai/lib"
)