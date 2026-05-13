"""카메라 디바이스 경로를 받아 OpenCV로 시각화.

사용법:
    python utile/test_cam.py /dev/camera_0
    python utile/test_cam.py /dev/camera_d435_1
"""

import sys
import cv2


def main():
    # device = sys.argv[1] if len(sys.argv) >= 2 else "/dev/camera_0"
    device = "/dev/cam_top"

    import os, stat
    print(f"[1] 디바이스 경로: {device}")

    real = os.path.realpath(device)
    print(f"[2] 실제 경로: {real}")
    print(f"[3] 경로 존재 여부: {os.path.exists(real)}")

    try:
        st = os.stat(real)
        print(f"[4] 파일 모드: {oct(stat.S_IMODE(st.st_mode))}")
        print(f"[5] uid={st.st_uid}  gid={st.st_gid}")
    except Exception as e:
        print(f"[4] stat 실패: {e}")

    import grp, pwd
    try:
        gid = os.stat(real).st_gid
        grp_name = grp.getgrgid(gid).gr_name
        my_groups = [g.gr_name for g in grp.getgrall() if pwd.getpwuid(os.getuid()).pw_name in g.gr_mem]
        my_groups.append(grp.getgrgid(os.getgid()).gr_name)
        print(f"[6] 디바이스 그룹: {grp_name}")
        print(f"[7] 현재 사용자 그룹 목록: {my_groups}")
        print(f"[8] 그룹 접근 가능: {grp_name in my_groups}")
    except Exception as e:
        print(f"[6] 그룹 확인 실패: {e}")

    print(f"[9] VideoCapture 시도 중...")
    cap = cv2.VideoCapture(device)
    print(f"[10] isOpened: {cap.isOpened()}")
    if not cap.isOpened():
        print(f"카메라 열기 실패: {device}")
        sys.exit(1)

    w   = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h   = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    print(f"[11] 해상도: {w}x{h}  FPS: {fps}")
    print(f"카메라 열림: {device}")
    print("종료: q")

    while True:
        ret, frame = cap.read()
        if not ret:
            print("프레임 읽기 실패")
            break

        cv2.imshow(device, frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()