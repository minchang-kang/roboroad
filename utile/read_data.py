import h5py
import numpy as np

with h5py.File("output/data_20260314_040859.hdf5", "r") as f:
    timestamps   = f["/timestamps"][:]
    master_joint = f["/observations/master_joint"][:]
    ur_joint     = f["/observations/ur_joint"][:]
    front_frames = f["/observations/images/front"][:]  # (T, H, W, 3)
    # hand_frames  = f["/observations/images/hand"][:]

print(f"프레임 수: {len(timestamps)}")
print(f"master_joint shape: {master_joint.shape}")
print(f"ur_joint shape: {ur_joint}")
print(f"front_frames shape: {front_frames.shape}")
