#!/bin/bash
set -e

# ── ttyUSB0 권한 설정 ─────────────────────────────────────
echo "[1/4] ttyUSB0 권한 설정..."
sudo chmod 666 /dev/ttyUSB0

# ── Latency Timer 확인 ────────────────────────────────────
echo "[2/4] 현재 Latency Timer 확인..."
current=$(cat /sys/bus/usb-serial/devices/ttyUSB0/latency_timer)
echo "  현재값: ${current}ms"
if [ "$current" -eq 16 ]; then
    echo "  경고: 16ms → 1ms로 변경합니다"
fi

# ── Latency Timer 1ms로 변경 ─────────────────────────────
echo "[3/4] Latency Timer → 1ms 설정..."
echo 1 | sudo tee /sys/bus/usb-serial/devices/ttyUSB0/latency_timer > /dev/null

# ── 결과 확인 ─────────────────────────────────────────────
echo "[4/4] 설정 결과 확인..."
ls -la /dev/ttyUSB0
echo "  Latency Timer: $(cat /sys/bus/usb-serial/devices/ttyUSB0/latency_timer)ms"

# ── 카메라 설정 ───────────────────────────────────────────
echo ""
echo "[5/5] 카메라 설정 (/dev/camera_0)..."
if [ ! -e /dev/camera_0 ]; then
    echo "  경고: /dev/camera_0 없음, 건너뜀"
else
    v4l2-ctl -d /dev/camera_0 -l
    v4l2-ctl -d /dev/camera_0 -c exposure_dynamic_framerate=0
    echo "  exposure_dynamic_framerate=0 설정 완료"
fi

echo ""
echo "세팅 완료."
