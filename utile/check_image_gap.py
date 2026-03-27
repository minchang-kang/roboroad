"""
check_image_gap.py — 이미지 프레임 누락 위치 확인

이미지 수가 timestamp 수보다 1개 적은 파일에서
누락이 첫 프레임인지, 마지막 프레임인지, 중간인지 추정합니다.

사용법:
  python3 utile/check_image_gap.py
"""

import glob
import os

import h5py
import numpy as np

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "output")

files = sorted(glob.glob(os.path.join(OUTPUT_DIR, "*.hdf5")))

problem_files = []
for f_path in files:
    with h5py.File(f_path, "r") as f:
        if "/observations/images/front" not in f:
            continue
        n_ts  = len(f["/timestamps"][:])
        n_img = f["/observations/images/front"].shape[0]
        if n_img != n_ts:
            problem_files.append(f_path)

print(f"이미지 불일치 파일: {len(problem_files)}개\n")
print(f"{'파일명':<40} {'ts':>5} {'img':>5}  누락 위치 추정")
print("-" * 75)

for f_path in problem_files:
    fname = os.path.basename(f_path)
    with h5py.File(f_path, "r") as f:
        ts     = f["/timestamps"][:].flatten()
        n_ts   = len(ts)
        n_img  = f["/observations/images/front"].shape[0]
        diff   = n_ts - n_img

        ts_diff    = np.diff(ts)
        median_dt  = np.median(ts_diff)

        # 정상 간격보다 2배 이상 큰 갭 → 그 위치에서 누락 추정
        gap_mask   = ts_diff > median_dt * 2.0
        gap_idxs   = np.where(gap_mask)[0]   # 갭 바로 앞 프레임 인덱스

        if len(gap_idxs) == 0:
            # 타임스탬프 간격에 이상 없음 → 첫 또는 마지막 프레임 누락
            # 첫 프레임 vs 마지막 프레임은 구분 불가 (타임스탬프만으론)
            # 통상적으로 마지막 프레임에서 발생
            location = f"마지막 프레임 (추정)"
        else:
            positions = []
            for idx in gap_idxs:
                if idx == 0:
                    positions.append("첫 번째 프레임")
                elif idx >= n_ts - 2:
                    positions.append("마지막 프레임")
                else:
                    pct = idx / n_ts * 100
                    positions.append(f"중간 (index={idx}, {pct:.0f}%)")
            location = " / ".join(positions)

        print(f"{fname:<40} {n_ts:>5} {n_img:>5}  → {location}")
