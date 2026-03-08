#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <thread>
#include <chrono>
#include <dynamixel_sdk.h> // Dynamixel SDK 헤더
#include <csignal> // For signal handling (Ctrl+C)

// Robotics Library 헤더
#include <rl/math/Vector.h>
#include <rl/mdl/Dynamic.h>
#include <rl/mdl/UrdfFactory.h>
#include <rl/mdl/Model.h>

bool running = true; // Global flag to control the main loop

// --- Dynamixel 설정 ---
#define DEVICENAME          "/dev/ttyUSB0"  // 포트 이름 (환경에 맞게 수정 필요: ttyUSB0, ttyACM0 등)
#define BAUDRATE            4000000         // Wizard에서 확인한 값으로 수정
#define PROTOCOL_VERSION    2.0

// XM430-W210-R Control Table Address
#define ADDR_RETURN_DELAY_TIME 9
#define ADDR_OPERATING_MODE 11
#define ADDR_TORQUE_ENABLE  64
#define ADDR_GOAL_CURRENT   102
#define ADDR_PRESENT_POSITION 132

// Data Byte Length
#define LEN_GOAL_CURRENT    2
#define LEN_PRESENT_POSITION 4

// 값 변환 상수
// XM430-W210: Stall Torque 3.0Nm @ 2.3A -> Kt = 1.30 Nm/A
// Unit: 2.69mA
const double DXL_KT = 1.30;             // Torque Constant (Nm/A)
const double DXL_UNIT_MA = 2.69;        // Current Unit (mA)
const int DXL_ZERO_POS = 2048;          // 0 rad 위치 (0~4095 중 중간값 가정)
const double DXL_POS_UNIT = 0.088 * (M_PI / 180.0); // 0.088 degree/step -> rad

const double SAFETY_GAIN = 0.8; // 안전을 위해 계산된 토크의 80%만 출력 (테스트 후 1.0으로 올리세요)

// Ctrl+C 핸들러
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down..." << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    // URDF 경로 설정
    std::string urdfFilePath = "/home/min/roboroad/urdf/UR3e_Master_Device.urdf";
    if (argc > 1) urdfFilePath = argv[1];

    // Dynamixel Port & Packet Handler 생성
    dynamixel::PortHandler *portHandler = dynamixel::PortHandler::getPortHandler(DEVICENAME);
    dynamixel::PacketHandler *packetHandler = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);

    dynamixel::GroupBulkRead groupRead(portHandler, packetHandler);
    dynamixel::GroupBulkWrite groupWrite(portHandler, packetHandler);

    // Register signal handler for Ctrl+C
    signal(SIGINT, signalHandler);

    // 모터 ID 배열 (URDF 순서와 일치해야 함)
    std::vector<uint8_t> dxl_ids = {1, 2, 3, 4, 5, 6};

    try {
        // 1. 모델 로드 (UrdfFactory)
        rl::mdl::UrdfFactory factory;
        std::shared_ptr<rl::mdl::Model> model(factory.create(urdfFilePath));

        // 2. Dynamic 모델로 캐스팅
        rl::mdl::Dynamic* dynamicModel = dynamic_cast<rl::mdl::Dynamic*>(model.get());
        if (!dynamicModel) {
            std::cerr << "Error: 모델을 Dynamic으로 변환할 수 없습니다." << std::endl;
            return -1;
        }

        const std::size_t dof = dynamicModel->getDof();
        std::cout << "Robot DOF: " << dof << std::endl;

        rl::math::Vector q(dof);
        rl::math::Vector tauGravity(dof);
        q.setZero();

        // --- Dynamixel 연결 및 설정 ---
        if (!portHandler->openPort()) {
            std::cerr << "Failed to open the port! (" << DEVICENAME << ")" << std::endl;
            return -1;
        }
        if (!portHandler->setBaudRate(BAUDRATE)) {
            std::cerr << "Failed to set the baudrate!" << std::endl;
            return -1;
        }
        std::cout << "Dynamixel Connected. Setup..." << std::endl;

        // 모터 설정
        for (const auto& id : dxl_ids) {
            uint8_t dxl_error = 0;
            int dxl_comm_result = COMM_TX_FAIL;

            // 1. 토크 끄기 (설정 변경 위해)
            dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, id, ADDR_TORQUE_ENABLE, 0, &dxl_error);
            if (dxl_comm_result != COMM_SUCCESS) {
                std::cerr << "[Setup Error ID " << (int)id << "] Disable Torque: " << packetHandler->getTxRxResult(dxl_comm_result) << std::endl;
            }
            
            // 2. Return Delay Time을 0으로 설정 (고속 통신 필수)
            // 파이썬 스크립트에는 이 과정이 있어서 잘 되었던 것입니다.
            dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, id, ADDR_RETURN_DELAY_TIME, 0, &dxl_error);
            if (dxl_comm_result != COMM_SUCCESS) {
                std::cerr << "[Setup Error ID " << (int)id << "] Set Return Delay: " << packetHandler->getTxRxResult(dxl_comm_result) << std::endl;
            }

            // 2. 전류 제어 모드(Current Control Mode: 0)로 설정
            // 주의: XM430은 Operating Mode 0번이 Current Control입니다.
            dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, id, ADDR_OPERATING_MODE, 0, &dxl_error);
            if (dxl_comm_result != COMM_SUCCESS) {
                std::cerr << "[Setup Error ID " << (int)id << "] Set Mode: " << packetHandler->getTxRxResult(dxl_comm_result) << std::endl;
            }
            
            // 3. 토크 켜기
            dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, id, ADDR_TORQUE_ENABLE, 1, &dxl_error);
            if (dxl_comm_result != COMM_SUCCESS) {
                std::cerr << "[Setup Error ID " << (int)id << "] Enable Torque: " << packetHandler->getTxRxResult(dxl_comm_result) << std::endl;
            }

            // BulkRead 파라미터 추가
            if (!groupRead.addParam(id, ADDR_PRESENT_POSITION, LEN_PRESENT_POSITION)) {
                std::cerr << "Failed to add BulkRead param for ID " << id << std::endl;
                return -1;
            }
        }

        std::cout << "Start Gravity Compensation Loop! (Press Ctrl+C to stop)" << std::endl;

        // 제어 루프
        while (running) {
            // 1. 현재 위치 읽기 (Sync/Bulk Read)
            int dxl_comm_result = groupRead.txRxPacket();
            if (dxl_comm_result != COMM_SUCCESS) {
                std::cerr << packetHandler->getTxRxResult(dxl_comm_result) << std::endl;
                // 통신 실패 시 잠시 대기 후 다음 루프 시도
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            for (size_t i = 0; i < dof; ++i) {
                uint8_t id = dxl_ids[i];
                if (groupRead.isAvailable(id, ADDR_PRESENT_POSITION, LEN_PRESENT_POSITION)) {
                    int32_t present_pos = (int32_t)groupRead.getData(id, ADDR_PRESENT_POSITION, LEN_PRESENT_POSITION);
                    
                    // Raw 값(0~4095) -> Radian 변환
                    // 2048을 0도로 가정. 방향이 반대라면 부호를 바꿔야 함 (-1.0 *)
                    q[i] = (present_pos - DXL_ZERO_POS) * DXL_POS_UNIT; 
                }
            }

            // 2. 중력 보상 토크 계산
            dynamicModel->setPosition(q);
            dynamicModel->calculateGravity(tauGravity);

            // 3. 토크 -> 전류 변환 및 전송
            groupWrite.clearParam();
            
            // 화면 출력용 (가끔 출력)
            static int print_cnt = 0;
            if (print_cnt++ % 100 == 0) {
                printf("J1: %.2f Nm, J2: %.2f Nm ... \r", tauGravity[0], tauGravity[1]);
                fflush(stdout);
            }

            for (size_t i = 0; i < dof; ++i) {
                uint8_t id = dxl_ids[i];
                double target_torque = tauGravity[i] * SAFETY_GAIN; // 안전 게인 적용

                // 공식: Current(mA) = (Torque / Kt) * 1000
                //       RawVal = Current(mA) / Unit(mA)
                double target_current_ma = (target_torque / DXL_KT) * 1000.0;
                int16_t goal_current_raw = (int16_t)(target_current_ma / DXL_UNIT_MA);

                // 전류 제한 (XM430-W210 최대 전류 약 2.3A = 2300mA -> Raw 855 정도)
                // 안전을 위해 Raw값 800으로 클리핑
                if (goal_current_raw > 800) goal_current_raw = 800;
                if (goal_current_raw < -800) goal_current_raw = -800;

                // 바이트 분리 (Little Endian)
                uint8_t param_goal_current[2];
                param_goal_current[0] = DXL_LOBYTE(goal_current_raw);
                param_goal_current[1] = DXL_HIBYTE(goal_current_raw);

                groupWrite.addParam(id, ADDR_GOAL_CURRENT, LEN_GOAL_CURRENT, param_goal_current);
            }

            // 일괄 전송
            groupWrite.txPacket();

            // 주기 조절 (약 100Hz)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -1;
    }

    // --- 프로그램 종료 시 토크 끄기 ---
    std::cout << "Disabling torque on all motors..." << std::endl;
    for (const auto& id : dxl_ids) {
        packetHandler->write1ByteTxRx(portHandler, id, ADDR_TORQUE_ENABLE, 0);
    }
    portHandler->closePort();
    std::cout << "Shutdown complete." << std::endl;

    return 0;
}
