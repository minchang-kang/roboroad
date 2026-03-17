"""
graph.py — DXL(마스터) vs UR(슬레이브) 관절각도 비교 그래프

비교 대상:
  servo_cmd[1~6]_rad : DXL 엔코더 → UR 좌표계 변환 명령값  (마스터)
  ur_q[1~6]_rad      : UR 실제 관절각도                    (슬레이브)
  error              : servo_cmd - ur_q                   (추종 오차)

사용법:
  python3 graph.py                          # 전체 구간
  python3 graph.py --start 5 --end 15       # 5~15초 구간만
  python3 graph.py --joints 1 2 3           # J1 J2 J3만
  python3 graph.py --start 5 --end 15 --joints 1 2 3
"""

import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import os

# ============================================================================
# CSV 경로 — 필요 시 변경
# ============================================================================
CSV_DIR  = os.path.join(os.path.dirname(__file__), "jog")
CSV_FILE = None   # None 이면 jog/ 폴더의 가장 최신 파일 자동 선택
PNG_DIR  = os.path.join(os.path.dirname(__file__), "plots_tracking_analysis")

# ============================================================================
# 인자 파싱
# ============================================================================
parser = argparse.ArgumentParser()
parser.add_argument("--csv",    type=str,   default=None,      help="CSV 파일 경로 (생략 시 최신 파일)")
parser.add_argument("--start",  type=float, default=None,      help="시작 시간 [s] (생략 시 전체)")
parser.add_argument("--end",    type=float, default=None,      help="종료 시간 [s] (생략 시 전체)")
parser.add_argument("--joints", type=int,   nargs="+",
                    default=[1,2,3,4,5,6],                     help="표시할 조인트 번호 (예: 1 2 3)")
args = parser.parse_args()

# ============================================================================
# CSV 로드
# ============================================================================
if args.csv:
    csv_path = args.csv
elif CSV_FILE:
    csv_path = os.path.join(CSV_DIR, CSV_FILE)
else:
    # jog/ 폴더에서 가장 최신 integrated.csv 자동 선택
    files = sorted([
        f for f in os.listdir(CSV_DIR) if f.endswith("_integrated.csv")
    ])
    if not files:
        raise FileNotFoundError(f"jog/ 폴더에 *_integrated.csv 파일이 없습니다: {CSV_DIR}")
    csv_path = os.path.join(CSV_DIR, files[-1])

print(f"[INFO] 로드: {csv_path}")
df = pd.read_csv(csv_path)

# ============================================================================
# 타임스탬프 → 상대 시간 [s]
# ============================================================================
df["time_s"] = (df["log_ts_ns"] - df["log_ts_ns"].iloc[0]) / 1e9

# ============================================================================
# 시간 구간 필터
# ============================================================================
if args.start is not None:
    df = df[df["time_s"] >= args.start]
if args.end is not None:
    df = df[df["time_s"] <= args.end]

if df.empty:
    raise ValueError("지정한 시간 구간에 데이터가 없습니다.")

print(f"[INFO] 구간: {df['time_s'].iloc[0]:.2f}s ~ {df['time_s'].iloc[-1]:.2f}s  ({len(df)} 샘플)")

# ============================================================================
# 조인트별 데이터 추출
# ============================================================================
joints = args.joints
t = df["time_s"].values

cmd  = {j: df[f"servo_cmd{j}_rad"].values for j in joints}
ur_q = {j: df[f"ur_q{j}_rad"].values      for j in joints}

# 시작 시점 오프셋 제거: 초기 각도 차이를 빼서 순수 추종 오차만 표시
init_offset = {j: cmd[j][0] - ur_q[j][0] for j in joints}
ur_q = {j: ur_q[j] + init_offset[j]      for j in joints}
err  = {j: cmd[j] - ur_q[j]              for j in joints}

# ============================================================================
# 그래프 — 조인트마다 2행 (각도 / 오차)
# ============================================================================
n = len(joints)
# 조인트마다 행 하나, 열은 [각도비교 | 추종오차] 두 개
fig, axes = plt.subplots(n, 2, figsize=(16, 3.8 * n))
if n == 1:
    axes = [axes]   # 단일 조인트일 때도 리스트로 통일

csv_name = os.path.basename(csv_path)
fig.suptitle(f"DXL (Master) vs UR (Slave) — Joint Tracking\n{csv_name}",
             fontsize=12, fontweight="bold", y=1.01)

for row, j in enumerate(joints):
    ax_ang, ax_err = axes[row]

    # ── 왼쪽: 각도 비교 ──────────────────────────────────────────────────
    ax_ang.plot(t, np.degrees(cmd[j]),  label="DXL cmd (master)", linewidth=1.2)
    ax_ang.plot(t, np.degrees(ur_q[j]), label="UR actual (slave)",
                linewidth=1.2, linestyle="--", color="darkorange")
    ax_ang.set_title(f"J{j}  Joint Angle", fontsize=10, pad=6)
    ax_ang.set_xlabel("Time [s]", fontsize=8)
    ax_ang.set_ylabel("Angle [deg]", fontsize=8)
    ax_ang.tick_params(labelsize=8)
    ax_ang.legend(fontsize=8, loc="upper left")
    ax_ang.grid(True, alpha=0.3)

    # ── 오른쪽: 추종 오차 ────────────────────────────────────────────────
    ax_err.plot(t, np.degrees(err[j]), color="tomato", linewidth=1.0)
    ax_err.axhline(0, color="gray", linewidth=0.8, linestyle="--")
    ax_err.fill_between(t, np.degrees(err[j]), 0, alpha=0.15, color="tomato")
    ax_err.set_title(f"J{j}  Tracking Error (cmd - actual)", fontsize=10, pad=6)
    ax_err.set_xlabel("Time [s]", fontsize=8)
    ax_err.set_ylabel("Error [deg]", fontsize=8)
    ax_err.tick_params(labelsize=8)
    ax_err.grid(True, alpha=0.3)

    # RMS 오차 텍스트 — 오른쪽 아래로 이동해 타이틀과 안 겹치게
    rms = np.sqrt(np.mean(err[j]**2))
    ax_err.text(0.98, 0.05, f"RMS = {np.degrees(rms):.3f} deg",
                transform=ax_err.transAxes, ha="right", va="bottom",
                fontsize=8, color="darkred",
                bbox=dict(boxstyle="round,pad=0.3", facecolor="lightyellow", alpha=0.8))

# ============================================================================
# 요약 출력
# ============================================================================
print(f"\n{'Joint':>6} {'Max Err [deg]':>14} {'RMS Err [deg]':>14} {'Mean Err [deg]':>15}")
print("-" * 52)
for j in joints:
    max_e  = np.degrees(np.max(np.abs(err[j])))
    rms_e  = np.degrees(np.sqrt(np.mean(err[j]**2)))
    mean_e = np.degrees(np.mean(err[j]))
    print(f"  J{j}   {max_e:>13.3f}  {rms_e:>13.3f}  {mean_e:>14.3f}")

# ============================================================================
# 저장
# ============================================================================
os.makedirs(PNG_DIR, exist_ok=True)
csv_stem  = os.path.splitext(csv_name)[0]
save_path = os.path.join(PNG_DIR, f"{csv_stem}.png")

plt.tight_layout()
plt.savefig(save_path, dpi=150, bbox_inches="tight")
print(f"\n[INFO] 저장 완료: {save_path}")

# ============================================================================
# [그래프 2] 오차 집중 구간 — 임계값 초과 구간만 표시
# ============================================================================
# 임계값: 전체 조인트 중 RMS의 1.5배, 최소 2도 보장
ERR_THRESHOLD_DEG = max(
    2.0,
    1.5 * max(np.degrees(np.sqrt(np.mean(err[j]**2))) for j in joints)
)
MARGIN_S   = 0.5   # 구간 전후 여유 [s]
MERGE_GAP  = 1.0   # 이 간격 이하 구간은 하나로 합침 [s]

print(f"\n[INFO] 오차 집중 임계값: {ERR_THRESHOLD_DEG:.2f} deg")

# 각 샘플에서 임의 조인트 하나라도 임계값 초과하면 True
any_over = np.zeros(len(t), dtype=bool)
for j in joints:
    any_over |= np.abs(np.degrees(err[j])) > ERR_THRESHOLD_DEG

# 연속 구간(시작/끝 인덱스) 추출
def find_segments(mask):
    segs = []
    in_seg = False
    for i, v in enumerate(mask):
        if v and not in_seg:
            start_i = i
            in_seg = True
        elif not v and in_seg:
            segs.append((start_i, i - 1))
            in_seg = False
    if in_seg:
        segs.append((start_i, len(mask) - 1))
    return segs

raw_segs = find_segments(any_over)

# 인접 구간 병합 (MERGE_GAP 이하 간격)
def merge_segments(segs, t, gap_s):
    if not segs:
        return []
    merged = [segs[0]]
    for s, e in segs[1:]:
        if t[s] - t[merged[-1][1]] <= gap_s:
            merged[-1] = (merged[-1][0], e)
        else:
            merged.append((s, e))
    return merged

segs = merge_segments(raw_segs, t, MERGE_GAP)

# 여유(MARGIN_S) 적용
def expand_segment(s_i, e_i, t, margin_s):
    t_s = max(t[0],   t[s_i] - margin_s)
    t_e = min(t[-1],  t[e_i] + margin_s)
    return t_s, t_e

if not segs:
    print("[INFO] 임계값 초과 구간 없음 — 오차 집중 그래프 생략")
else:
    print(f"[INFO] 오차 구간 {len(segs)}개 발견")
    n_segs = len(segs)
    n_joints = len(joints)

    # 행: 구간, 열: 조인트
    fig2, axes2 = plt.subplots(n_segs, n_joints,
                                figsize=(5.5 * n_joints, 3.2 * n_segs),
                                squeeze=False)

    csv_name2 = os.path.basename(csv_path)
    fig2.suptitle(
        f"Error-Focus View (threshold = {ERR_THRESHOLD_DEG:.1f} deg)\n{csv_name2}",
        fontsize=11, fontweight="bold"
    )

    for row, (s_i, e_i) in enumerate(segs):
        t_s, t_e = expand_segment(s_i, e_i, t, MARGIN_S)
        mask = (t >= t_s) & (t <= t_e)
        t_seg = t[mask]

        for col, j in enumerate(joints):
            ax = axes2[row][col]
            cmd_seg = np.degrees(cmd[j][mask])
            urq_seg = np.degrees(ur_q[j][mask])
            err_seg = np.degrees(err[j][mask])

            ax.plot(t_seg, cmd_seg, label="DXL cmd",    linewidth=1.2)
            ax.plot(t_seg, urq_seg, label="UR actual",
                    linewidth=1.2, linestyle="--", color="darkorange")
            ax.fill_between(t_seg, cmd_seg, urq_seg,
                            alpha=0.15, color="tomato", label="error")

            rms_seg = np.sqrt(np.mean((err[j][mask])**2))
            max_seg = np.max(np.abs(err[j][mask]))
            ax.set_title(
                f"Seg{row+1} · J{j}  [{t_s:.1f}s~{t_e:.1f}s]\n"
                f"RMS={np.degrees(rms_seg):.2f}°  Max={np.degrees(max_seg):.2f}°",
                fontsize=8, pad=4
            )
            ax.set_xlabel("Time [s]", fontsize=7)
            ax.set_ylabel("Angle [deg]", fontsize=7)
            ax.tick_params(labelsize=7)
            ax.legend(fontsize=7, loc="upper left")
            ax.grid(True, alpha=0.3)

            # 임계값 초과 구간 음영
            over_mask = np.abs(err_seg) > ERR_THRESHOLD_DEG
            if over_mask.any():
                ax.fill_between(t_seg, ax.get_ylim()[0], ax.get_ylim()[1],
                                where=over_mask, alpha=0.07, color="red",
                                transform=ax.get_xaxis_transform())

    plt.tight_layout()
    save_path2 = os.path.join(PNG_DIR, f"{csv_stem}_error_focus.png")
    plt.savefig(save_path2, dpi=150, bbox_inches="tight")
    print(f"[INFO] 저장 완료: {save_path2}")

plt.show()
