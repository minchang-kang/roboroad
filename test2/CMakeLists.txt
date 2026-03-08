cmake_minimum_required(VERSION 3.16)
project(GravityCompensation)

set(CMAKE_CXX_STANDARD 17)

# ─── Robotics Library (소스 직접 사용) ───────────────
set(RL_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(RL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RL_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(RL_BUILD_SG OFF CACHE BOOL "" FORCE)
set(RL_BUILD_HAL OFF CACHE BOOL "" FORCE)
set(RL_BUILD_KIN OFF CACHE BOOL "" FORCE)
set(RL_BUILD_PLAN OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/rl rl_build)

# ─── DynamixelSDK (소스 직접 사용) ───────────────────
set(DYNAMIXEL_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_bulk_read.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_bulk_write.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_sync_read.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_sync_write.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/group_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/packet_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/port_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/port_handler_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/protocol2_packet_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/src/dynamixel_sdk/protocol1_packet_handler.cpp
)

add_executable(gravity_comp main.cpp ${DYNAMIXEL_SRC})

target_include_directories(gravity_comp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/DynamixelSDK/c++/include/dynamixel_sdk
)

target_compile_definitions(gravity_comp PRIVATE LINUX _GNU_SOURCE)

target_link_libraries(gravity_comp PRIVATE mdl math xml)

# (선택 사항) 컴파일러 경고 옵션 추가
if(MSVC)
    target_compile_options(gravity_comp PRIVATE /W4)
else()
    target_compile_options(gravity_comp PRIVATE -Wall -Wextra)
endif()
