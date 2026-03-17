import h5py
import numpy as np
import sys
import os

if len(sys.argv) < 2:
    print("사용법: python3 utile/read_data.py <hdf5_파일_경로>")
    sys.exit(1)

file_path = sys.argv[1]

if not os.path.exists(file_path):
    print(f"오류: 파일을 찾을 수 없습니다 -> {file_path}")
    sys.exit(1)

with h5py.File(file_path, "r") as f:
    timestamps   = f["/timestamps"][:]
    master_joint = f["/observations/master_joint"][:]
    ur_joint     = f["/observations/ur_joint"][:]

    if "/observations/images/front" in f:
        front_frames = f["/observations/images/front"][:]  # (T, H, W, 3)
    else:
        front_frames = np.array([])
        print("경고: HDF5 파일에 'front' 카메라 데이터가 없습니다.")
    # hand_frames  = f["/observations/images/hand"][:]

print(f"프레임 수: {len(timestamps)}")
print(f"master_joint shape: {master_joint.shape}")
print(f"ur_joint shape: {ur_joint.shape}")
print(f"front_frames shape: {front_frames.shape}")
