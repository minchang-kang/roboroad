1. 의존성
``` bash
sudo apt-get install libboost-all-dev libeigen3-dev libxml2-dev libxslt1-dev
```

2.

---

## 시스템 아키텍처

### 스레드 구조

```
Non RT thread
  ├──

PREEMPT_RT thread (SCHED_FIFO)
  ├── Dynamixel 읽기
  ├── RTDE 읽기
  └── 데이터 저장
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
