"""HDF5 episode data visualizer for roboroad recordings."""

import sys
from pathlib import Path

import h5py
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np


JOINT_NAMES = ["Base", "Shoulder", "Elbow", "Wrist1", "Wrist2", "Wrist3"]

# ── VSCode에서 직접 실행할 때 여기에 경로를 지정 ──────────────
DEFAULT_HDF5_PATH = "/home/mugun/Desktop/roboroad/output/Epoch_003.hdf5"


def load(path: Path):
    with h5py.File(path, "r") as f:
        data = {
            "front":        f["observations/images/front"][:],
            "back":         f["observations/images/back"][:],
            "hand":         f["observations/images/hand"][:],
            "master_joint": f["observations/master_joint"][:],
            "ur_joint":     f["observations/ur_joint"][:],
            "mouse_action": f["observations/mouse_action"][:].squeeze(),
            "timestamps":   f["timestamps"][:].squeeze(),
        }
    return data


def print_summary(data, path):
    ts = data["timestamps"]
    duration_us = ts[-1] - ts[0]
    fps = len(ts) / (duration_us / 1_000_000) if duration_us > 0 else 0

    print("=" * 50)
    print(f"  File       : {path.name}")
    print(f"  Frames     : {len(ts)}")
    print(f"  Duration   : {duration_us / 1_000_000:.2f} s")
    print(f"  Avg FPS    : {fps:.1f}")
    print(f"  Images     : front / back / hand  {data['front'].shape[1]}x{data['front'].shape[2]}")
    print(f"  UR joint   : min={data['ur_joint'].min():.3f}  max={data['ur_joint'].max():.3f}")
    print(f"  Master jnt : min={data['master_joint'].min():.3f}  max={data['master_joint'].max():.3f}")
    print(f"  Mouse act  : unique values = {np.unique(data['mouse_action'])}")
    print("=" * 50)


def plot_joints(data, ax_ur, ax_master):
    ts = (data["timestamps"] - data["timestamps"][0]) / 1_000_000.0  # → seconds

    for i, name in enumerate(JOINT_NAMES):
        ax_ur.plot(ts, np.degrees(data["ur_joint"][:, i]), label=name)
        ax_master.plot(ts, np.degrees(data["master_joint"][:, i]), label=name)

    for ax, title in [(ax_ur, "UR Joint Angles"), (ax_master, "Master Joint Angles")]:
        ax.set_title(title)
        ax.set_xlabel("Time (s)")
        ax.set_ylabel("Angle (deg)")
        ax.legend(fontsize=7, ncol=2)
        ax.grid(alpha=0.3)


def plot_mouse(data, ax):
    ts = (data["timestamps"] - data["timestamps"][0]) / 1_000_000.0
    ax.step(ts, data["mouse_action"], where="post", color="tab:orange")
    ax.set_title("Mouse Action")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Value")
    ax.grid(alpha=0.3)


def plot_sample_images(data):
    cameras = ["front", "back", "hand"]
    n = len(data["front"])
    sample_frames = [int(n * r) for r in [0, 0.25, 0.5, 0.75, 1.0]]
    sample_frames[-1] = min(sample_frames[-1], n - 1)
    frames = sorted(set(sample_frames))
    n_frames = len(frames)

    fig, axes = plt.subplots(3, n_frames, figsize=(3 * n_frames, 9))
    fig.suptitle("Sample Frames (front / back / hand)", fontsize=13)

    for col, fi in enumerate(frames):
        ts_sec = (data["timestamps"][fi] - data["timestamps"][0]) / 1_000_000.0
        for row, cam in enumerate(cameras):
            ax = axes[row][col]
            ax.imshow(data[cam][fi])
            ax.axis("off")
            if row == 0:
                ax.set_title(f"t={ts_sec:.1f}s", fontsize=9)
            if col == 0:
                ax.set_ylabel(cam, fontsize=9)

    plt.tight_layout()


def main():
    if len(sys.argv) >= 2:
        path = Path(sys.argv[1])
    else:
        path = Path(DEFAULT_HDF5_PATH)

    if not path.exists():
        print(f"File not found: {path}")
        sys.exit(1)

    print(f"Loading {path} ...")
    data = load(path)

    print_summary(data, path)

    # ── Figure 1: joints + mouse ───────────────────────────────────────────────
    fig = plt.figure(figsize=(14, 9))
    fig.suptitle(f"{path.name}  —  Joint & Action Overview", fontsize=13)
    gs = gridspec.GridSpec(3, 1, hspace=0.45)

    ax_ur     = fig.add_subplot(gs[0])
    ax_master = fig.add_subplot(gs[1])
    ax_mouse  = fig.add_subplot(gs[2])

    plot_joints(data, ax_ur, ax_master)
    plot_mouse(data, ax_mouse)

    # ── Figure 2: sample images ────────────────────────────────────────────────
    plot_sample_images(data)

    plt.show()


if __name__ == "__main__":
    main()
