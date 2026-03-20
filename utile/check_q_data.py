import h5py
import numpy as np
import sys
import os

def main():
    if len(sys.argv) < 2:
        print("사용법: python3 utile/check_q_data.py <hdf5_파일_경로>")
        sys.exit(1)

    file_path = sys.argv[1]

    if not os.path.exists(file_path):
        print(f"오류: 파일을 찾을 수 없습니다 -> {file_path}")
        sys.exit(1)

    with h5py.File(file_path, "r") as f:
        timestamps   = f["/timestamps"][:]
        master_joint = f["/observations/master_joint"][:]
        ur_joint     = f["/observations/ur_joint"][:]

        print("=== 데이터 동기화 & Queue 동작 확인 ===")
        print(f"저장된 총 프레임 수: {len(timestamps)}")
        
        # 1. UR 데이터 갱신 확인 (Queue가 정상 작동하면 로봇이 움직일 때 값이 변해야 함)
        ur_diff = np.diff(ur_joint, axis=0)
        ur_moving = np.any(np.abs(ur_diff) > 1e-5)
        
        if ur_moving:
            print("✅ UR 데이터: 정상적으로 값이 변화하고 있습니다. (Queue 동기화 성공)")
        else:
            print("❌ UR 데이터: 변화가 없습니다. (데이터가 멈춰있거나 로봇을 움직이지 않았음)")

        # 2. 마스터 데이터 확인 (현재 임시 비활성화로 값이 0이어야 함)
        if np.sum(np.abs(master_joint)) == 0:
            print("⚠️ 마스터 데이터: 모두 0입니다. (마스터 비활성화 상태 정상 반영)")
        else:
            print("✅ 마스터 데이터: 값이 존재합니다.")

        # 3. 비전 카메라 확인
        if "/observations/images/front" in f:
            front_frames = f["/observations/images/front"]
            print(f"✅ 비전 데이터: front 카메라 {front_frames.shape} 정상 저장됨")

        # 4. 처음 5프레임 샘플 출력
        print("\n[처음 5개 프레임 타임스탬프 & UR J1 확인]")
        for i in range(min(5, len(timestamps))):
            print(f"  Frame {i}: timestamp={timestamps[i]}, UR J1={ur_joint[i][0]:.4f}")

if __name__ == "__main__":
    main()