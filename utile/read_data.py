import h5py
import numpy as np
import sys
import os
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("사용법: python3 utile/read_data.py <hdf5_파일_경로>")
    sys.exit(1)

file_path = sys.argv[1]

if not os.path.exists(file_path):
    print(f"오류: 파일을 찾을 수 없습니다 -> {file_path}")
    sys.exit(1)

with h5py.File(file_path, "r") as f:
    timestamps   = f["/timestamps"][:].flatten()
    master_joint = f["/observations/master_joint"][:]
    ur_joint     = f["/observations/ur_joint"][:]

    if "/observations/images/front" in f:
        front_frames = f["/observations/images/front"][:]
    else:
        front_frames = np.array([])
        print("경고: HDF5 파일에 'front' 카메라 데이터가 없습니다.")

    if "/observations/mouse_action" in f:
        mouse_action = f["/observations/mouse_action"][:].flatten()
    else:
        mouse_action = np.array([])
        print("경고: HDF5 파일에 'mouse_action' 데이터가 없습니다.")

print(f"프레임 수: {len(timestamps)}")
print(f"master_joint shape: {master_joint.shape}")
print(f"ur_joint shape: {ur_joint.shape}")
print(f"front_frames shape: {front_frames.shape}")
if len(mouse_action) > 0:
    print(f"mouse_action shape: {mouse_action.shape}  (ON: {mouse_action.sum()}프레임, OFF: {(mouse_action==0).sum()}프레임)")

# timestamp를 0 기준 상대 시간(초)으로 변환
t_sec = (timestamps - timestamps[0]) / 1e6
num_joints = master_joint.shape[1]

# ─── 1. Master Joint ──────────────────────────────────────────────────────────
fig, axes = plt.subplots(num_joints, 1, figsize=(12, 2.5 * num_joints), sharex=True)
fig.suptitle("Master Joint Angles", fontsize=14)
for i, ax in enumerate(axes):
    ax.plot(t_sec, master_joint[:, i])
    ax.set_ylabel(f"Joint {i+1}\n[rad]")
    ax.grid(True)
axes[-1].set_xlabel("Time [s]")
plt.tight_layout()

# ─── 2. UR Joint ─────────────────────────────────────────────────────────────
fig, axes = plt.subplots(num_joints, 1, figsize=(12, 2.5 * num_joints), sharex=True)
fig.suptitle("UR Joint Angles", fontsize=14)
for i, ax in enumerate(axes):
    ax.plot(t_sec, ur_joint[:, i], color="tab:orange")
    ax.set_ylabel(f"Joint {i+1}\n[rad]")
    ax.grid(True)
axes[-1].set_xlabel("Time [s]")
plt.tight_layout()

# ─── 3. Timestamp 간격 ────────────────────────────────────────────────────────
dt_ms = np.diff(timestamps) / 1e3  # microseconds → milliseconds
fig, ax = plt.subplots(figsize=(12, 4))
ax.plot(t_sec[1:], dt_ms, color="tab:green", linewidth=0.8)
ax.axhline(np.mean(dt_ms), color="red", linestyle="--", label=f"mean: {np.mean(dt_ms):.2f} ms")
ax.set_xlabel("Time [s]")
ax.set_ylabel("Δt [ms]")
ax.set_title("Timestamp Interval")
ax.legend()
ax.grid(True)
plt.tight_layout()

# ─── 4. Mouse Action ─────────────────────────────────────────────────────────
if len(mouse_action) > 0:
    fig, ax = plt.subplots(figsize=(12, 2))
    ax.plot(t_sec, mouse_action, color="tab:red", linewidth=1.0)
    ax.set_xlabel("Time [s]")
    ax.set_ylabel("Mouse\nAction")
    ax.set_yticks([0, 1])
    ax.set_yticklabels(["OFF", "ON"])
    ax.set_title("Mouse Action (Spray)")
    ax.grid(True)
    plt.tight_layout()

plt.show()
