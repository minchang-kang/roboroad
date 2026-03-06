cmake_minimum_required(VERSION 3.16)
project(roboroad)

set(CMAKE_CXX_STANDARD 17)

# ─── ur_rtde ─────────────────────────────────────────
add_subdirectory(3rdparty/ur_rtde)

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
    3rdparty/DynamixelSDK/c++/src/dynamixel_easy_sdk/connector.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_easy_sdk/control_table.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_easy_sdk/motor.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_easy_sdk/dynamixel_error.cpp
    3rdparty/DynamixelSDK/c++/src/dynamixel_easy_sdk/group_executor.cpp
)

# ─── 소스 파일 ──────────────────────────────────────
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${DYNAMIXEL_SRC}
)

# ─── include 경로 ────────────────────────────────────
target_include_directories(${PROJECT_NAME} PRIVATE
    src/
    3rdparty/DynamixelSDK/c++/include
    3rdparty/DynamixelSDK/c++/include/dynamixel_sdk
    3rdparty/DynamixelSDK/c++/include/dynamixel_easy_sdk
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    LINUX
    _GNU_SOURCE
    CONTROL_TABLE_PATH="/usr/local/share/dynamixel_sdk/control_table"
)
# ─── 링크 ────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME}
    rtde
    rt
)
