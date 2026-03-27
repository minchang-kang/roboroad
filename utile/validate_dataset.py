"""
validate_dataset.py — output/ 폴더의 HDF5 데이터셋 전체 검증

검증 항목:
  1. 프레임 수 일관성
  2. UR 관절 데이터 변화 여부 (정적 데이터 감지)
  3. 마스터 관절 데이터 유효성
  4. 타임스탬프 단조증가 & 간격 이상치
  5. 이미지 데이터 존재 여부 및 shape
  6. 관절값 범위 이상치 (NaN, Inf, 극단값)
  7. 녹화 길이 (너무 짧은 파일)
  8. 타임스탬프 갭으로 인한 실질 녹화시간 vs 총 시간 괴리

사용법:
  python3 utile/validate_dataset.py               # output/ 폴더 전체
  python3 utile/validate_dataset.py --verbose     # 각 파일 상세 출력
  python3 utile/validate_dataset.py --file output/data_xxx.hdf5  # 단일 파일
"""

import argparse
import glob
import os
import sys

import h5py
import numpy as np

# ─────────────────────────────────────────────────────────────────────────────
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "output")

parser = argparse.ArgumentParser()
parser.add_argument("--file",    type=str, default=None, help="단일 HDF5 파일 경로")
parser.add_argument("--verbose", action="store_true",    help="각 파일 상세 결과 출력")
args = parser.parse_args()

if args.file:
    files = [args.file]
else:
    files = sorted(glob.glob(os.path.join(OUTPUT_DIR, "*.hdf5")))

if not files:
    print("검증할 HDF5 파일이 없습니다.")
    sys.exit(1)

print(f"검증 대상: {len(files)}개 파일\n")

# ─────────────────────────────────────────────────────────────────────────────
PASS = "✅"
WARN = "⚠️ "
FAIL = "❌"

MIN_DURATION_S  = 2.0    # 이 미만이면 너무 짧은 에피소드로 경고
GAP_THRESH_S    = 1.0    # 타임스탬프 갭이 이 이상이면 끊김으로 판단

summary = {"total": 0, "pass": 0, "warn": 0, "fail": 0}
issue_files = []
stats = []   # (fname, n_frames, duration_s, status)

for f_path in files:
    fname = os.path.basename(f_path)
    issues = []   # (level, message)

    try:
        with h5py.File(f_path, "r") as f:
            # ── 필수 키 존재 확인 ────────────────────────────────────────────
            required = ["/timestamps", "/observations/master_joint", "/observations/ur_joint"]
            for key in required:
                if key not in f:
                    issues.append((FAIL, f"필수 키 없음: {key}"))

            if any(lvl == FAIL for lvl, _ in issues):
                # 필수 키 없으면 이후 검사 불가
                pass
            else:
                ts = f["/timestamps"][:]
                mj = f["/observations/master_joint"][:]
                ur = f["/observations/ur_joint"][:]
                n_frames = len(ts)

                # ── 1. 프레임 수 일관성 ──────────────────────────────────────
                shapes = {"timestamps": n_frames, "master_joint": len(mj), "ur_joint": len(ur)}
                if len(set(shapes.values())) > 1:
                    issues.append((FAIL, f"프레임 수 불일치: {shapes}"))

                # ── 2. UR 데이터 변화 여부 ───────────────────────────────────
                if n_frames > 1:
                    ur_diff = np.diff(ur, axis=0)
                    ur_max_delta = np.max(np.abs(ur_diff))
                    if ur_max_delta < 1e-5:
                        issues.append((WARN, f"UR 관절값 변화 없음 (max_delta={ur_max_delta:.2e}) — 로봇 정지 데이터?"))

                # ── 3. 마스터 데이터 유효성 ──────────────────────────────────
                mj_sum = np.sum(np.abs(mj))
                if mj_sum == 0.0:
                    issues.append((WARN, "마스터 관절값 전부 0 (마스터 비활성화 상태)"))

                # ── 4. 타임스탬프 단조증가 & 간격 이상치 ────────────────────
                if n_frames > 1:
                    ts_flat = ts.flatten()
                    ts_diff = np.diff(ts_flat)
                    if np.any(ts_diff <= 0):
                        n_bad = np.sum(ts_diff <= 0)
                        issues.append((FAIL, f"타임스탬프 역전/중복 {n_bad}곳"))

                    median_dt = np.median(ts_diff)
                    outlier_mask = np.abs(ts_diff - median_dt) > 5 * median_dt
                    n_outlier = outlier_mask.sum()
                    if n_outlier > 0:
                        max_dt = ts_diff[outlier_mask].max()
                        issues.append((WARN, f"타임스탬프 간격 이상치 {n_outlier}곳 (최대={max_dt:.0f}, 중간값={median_dt:.0f})"))

                # ── 5. 이미지 데이터 ─────────────────────────────────────────
                if "/observations/images/front" in f:
                    img_shape = f["/observations/images/front"].shape
                    if img_shape[0] != n_frames:
                        issues.append((FAIL, f"이미지 프레임 수 불일치: {img_shape[0]} vs {n_frames}"))
                else:
                    issues.append((WARN, "front 카메라 이미지 없음"))

                # ── 6. 관절값 NaN / Inf / 극단값 ────────────────────────────
                for name, arr in [("master_joint", mj), ("ur_joint", ur)]:
                    if np.any(np.isnan(arr)):
                        issues.append((FAIL, f"{name}에 NaN 존재"))
                    if np.any(np.isinf(arr)):
                        issues.append((FAIL, f"{name}에 Inf 존재"))
                    abs_max = np.max(np.abs(arr))
                    if abs_max > 2 * np.pi * 3:   # 3 회전 이상이면 의심
                        issues.append((WARN, f"{name} 극단값 감지: max_abs={abs_max:.3f} rad"))

                # ── 7. 녹화 길이 확인 ────────────────────────────────────────
                ts_flat = ts.flatten()
                duration_s = (ts_flat[-1] - ts_flat[0]) / 1e6
                if duration_s < MIN_DURATION_S:
                    issues.append((WARN, f"녹화 길이 너무 짧음: {duration_s:.2f}s"))

                # ── 8. 타임스탬프 갭 (실질 끊김) 감지 ───────────────────────
                if n_frames > 1:
                    ts_diff = np.diff(ts_flat)
                    gap_mask = ts_diff > GAP_THRESH_S * 1e6
                    if gap_mask.any():
                        gap_times = ts_diff[gap_mask] / 1e6
                        gap_positions = np.where(gap_mask)[0]
                        gap_desc = ", ".join(
                            f"{ts_flat[i]/1e6:.1f}s→{ts_flat[i+1]/1e6:.1f}s({g:.1f}s갭)"
                            for i, g in zip(gap_positions, gap_times)
                        )
                        real_s = duration_s - gap_times.sum()
                        issues.append((WARN,
                            f"녹화 중 끊김 {gap_mask.sum()}곳 "
                            f"(실질 녹화={real_s:.2f}s / 총={duration_s:.2f}s) [{gap_desc}]"))
                    else:
                        duration_s_real = duration_s  # 갭 없음

    except Exception as e:
        issues.append((FAIL, f"파일 읽기 오류: {e}"))
        duration_s = 0.0
        n_frames = 0

    # ── 결과 집계 ─────────────────────────────────────────────────────────────
    summary["total"] += 1
    has_fail = any(lvl == FAIL for lvl, _ in issues)
    has_warn = any(lvl == WARN for lvl, _ in issues)

    if has_fail:
        summary["fail"] += 1
        file_status = FAIL
        issue_files.append((fname, issues))
    elif has_warn:
        summary["warn"] += 1
        file_status = WARN
        issue_files.append((fname, issues))
    else:
        summary["pass"] += 1
        file_status = PASS

    stats.append((fname, n_frames, duration_s, file_status))

    if args.verbose or has_fail or has_warn:
        print(f"{file_status} {fname}  [{n_frames}frames / {duration_s:.2f}s]")
        for lvl, msg in issues:
            print(f"     {lvl} {msg}")
        if not issues:
            print(f"     모든 항목 통과")

# ─────────────────────────────────────────────────────────────────────────────
durations = [d for _, _, d, _ in stats if d > 0]
print()
print("=" * 60)
print(f"[검증 요약]  총 {summary['total']}개")
print(f"  {PASS} 정상:  {summary['pass']}개")
print(f"  {WARN} 경고:  {summary['warn']}개")
print(f"  {FAIL} 오류:  {summary['fail']}개")
print()
print(f"[녹화 길이 통계]")
print(f"  평균: {np.mean(durations):.2f}s")
print(f"  최소: {np.min(durations):.2f}s  /  최대: {np.max(durations):.2f}s")
print(f"  총 누적: {np.sum(durations):.1f}s  ({np.sum(durations)/60:.1f}분)")

if not args.verbose and issue_files:
    print("\n[문제 파일 목록]")
    for fname, issues in issue_files:
        print(f"  {fname}")
        for lvl, msg in issues:
            print(f"    {lvl} {msg}")
