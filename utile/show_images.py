"""HDF5 파일의 observation/images 아래 hand, top, front 이미지를 순서대로 출력."""

import sys
from pathlib import Path

import cv2
import h5py
import numpy as np

DEFAULT_HDF5_PATH = "/media/min/ORICO/data/Epoch_036.hdf5"   # 파일 경로로 변경하세요


def main():
    path = Path(sys.argv[1]) if len(sys.argv) >= 2 else Path(DEFAULT_HDF5_PATH)

    if not path.exists():
        print(f"File not found: {path}")
        sys.exit(1)

    print(f"Loading {path} ...")
    with h5py.File(path, "r") as f:
        front = f["observations/images/front"][:]
        top  = f["observations/images/top"][:]
        hand  = f["observations/images/hand"][:]

    n_frames = len(front)
    print(f"총 프레임 수: {n_frames}")
    print(f"  front shape : {front.shape}  dtype: {front.dtype}")
    print(f"  top  shape : {top.shape}  dtype: {top.dtype}")
    print(f"  hand  shape : {hand.shape}  dtype: {hand.dtype}")
    print()
    print("재생 중... (q: 종료 / 스페이스: 일시정지)")

    paused = False
    idx = 0

    while True:
        if not paused:
            f_img = cv2.cvtColor(front[idx], cv2.COLOR_RGB2BGR)
            b_img = cv2.cvtColor(top[idx],  cv2.COLOR_RGB2BGR)
            h_img = cv2.cvtColor(hand[idx],  cv2.COLOR_RGB2BGR)

            # 세 화면을 가로로 붙임
            row = np.concatenate([f_img, b_img, h_img], axis=1)

            # 프레임 번호 표시
            cv2.putText(row, f"frame {idx}/{n_frames-1}", (10, 24),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            # 카메라 이름 표시
            w = f_img.shape[1]
            for i, label in enumerate(["front", "top", "hand"]):
                cv2.putText(row, label, (i * w + 10, 50),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)

            cv2.imshow("front | top | hand", row)
            idx = (idx + 1) % n_frames

        key = cv2.waitKey(33) & 0xFF   # ~30 fps
        if key == ord('q'):
            break
        elif key == ord(' '):
            paused = not paused

    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
