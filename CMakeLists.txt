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
# ur_rtde, yaml-cpp 는 시스템 설치본 사용 (sudo make install 완료 상태)
find_package(ur_rtde REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(RL 0.7.0 COMPONENTS MATH MDL XML REQUIRED)

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
    ${DYNAMIXEL_SRC}
)


# ─── include 경로 ────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    src/
    3rdparty/DynamixelSDK/c++/include
    3rdparty/DynamixelSDK/c++/include/dynamixel_sdk
    ${OpenCV_INCLUDE_DIRS}
    ${HDF5_INCLUDE_DIRS}
    /usr/local/include/rl-0.7.0
    /usr/include/eigen3
    /usr/xenomai/include
    /usr/xenomai/include/cobalt
    /usr/xenomai/include/alchemy
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