1. 의존성
``` bash
sudo apt-get install libboost-all-dev libeigen3-dev libxml2-dev libxslt1-dev libopencv-dev libhdf5-dev
```

2.

---

## 시스템 아키텍처

### 스레드 구조

```
Non RT thread
  ├── Vision
  ├     ├── Front Cam
  ├     ├── Handle Cam
  ├── Teleop
  ├── keyboard input
  ├── fsr 확인
  ├── RTDE 읽기
  ├── Recorder
  └── Save

PREEMPT_RT thread (SCHED_FIFO)
  ├── Dynamixel 읽기
  └── 중력보상 계산    (pure computation, 타이밍 critical)
```

### 소스 구조

```
src/
  hal/
    dynamixel/            # 마스터 디바이스 HAL (Dynamixel SDK)
    ur/                   # UR 로봇 HAL (ur_rtde)
    fsr/                  # FSR 센서 HAL
    input/                # 키보드 입력 HAL
  data/
    recorder/             # 데이터 수집 및 저장 (모방학습용)
  teleop/                 # 원격 조작 로직
  core/
    gravity_compensation/ # 중력보상 계산 (Xenomai Hard RT)
  main.cpp
```

### 수집 데이터 (모방학습)

| 항목 | 출처 | 용도 |
|------|------|------|
| `master_q[6]` | Dynamixel | Action (사람의 행동) |
| `robot_q[6]` | UR RTDE `actual_q` | State (로봇 관절 각도) |
| `timestamp_ns` | `CLOCK_MONOTONIC` | Canonical timestamp |


## 비전 symlink 생성 가이드
1. rule 파일 생성
``` bash
sudo nano /etc/udev/rules.d/99-cameras.rules

```

2. 시러얼 넘버에 따른 이름 부여
안에 붙여넣기
``` bash
# /etc/udev/rules.d/99-cameras.rules
SUBSYSTEM=="video4linux", ENV{ID_SERIAL_SHORT}=="B2AFAD9F", ATTR{index}=="0", SYMLINK+="camera_0"
SUBSYSTEM=="video4linux", ENV{ID_SERIAL_SHORT}=="0210BD9F", ATTR{index}=="0", SYMLINK+="camera_1"
```

3. 적용
``` bash
sudo udevadm control --reload-rules && sudo udevadm trigger
ls /dev/camera*
```