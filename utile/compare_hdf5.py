import h5py
import numpy as np
import os
import glob

# output 폴더에서 hdf5 파일 목록을 시간순으로 가져오기
output_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "output")
files = sorted(glob.glob(os.path.join(output_dir, "*.hdf5")))

if len(files) < 2:
    print("비교할 hdf5 파일이 2개 이상 필요합니다.")
    exit(1)

# 최근 연속된 2개 파일 선택
file1_path = files[-2]
file2_path = files[-1]

print(f"파일 1: {os.path.basename(file1_path)}")
print(f"파일 2: {os.path.basename(file2_path)}")
print("=" * 60)


def load_file(path):
    data = {}
    with h5py.File(path, "r") as f:
        def collect(name, obj):
            if isinstance(obj, h5py.Dataset):
                data[name] = obj[()]
        f.visititems(collect)
    return data


data1 = load_file(file1_path)
data2 = load_file(file2_path)

# 공통 키 목록
keys1 = set(data1.keys())
keys2 = set(data2.keys())
common_keys = sorted(keys1 & keys2)

print(f"\n[공통 데이터셋 목록]")
for k in common_keys:
    print(f"  {k}")

print("\n" + "=" * 60)
print("[좌표값 비교]")

# 이미지 데이터는 제외하고 수치 데이터만 비교
skip_prefixes = ("observations/images",)

for key in common_keys:
    if any(key.startswith(p) for p in skip_prefixes):
        continue

    v1 = data1[key]
    v2 = data2[key]

    if not np.issubdtype(v1.dtype, np.number):
        continue

    print(f"\n[{key}]")
    print(f"  shape: {v1.shape} vs {v2.shape}")

    # 마지막 프레임 값 출력
    if v1.ndim >= 1:
        last1 = v1[-1] if v1.ndim == 1 else v1[-1]
        last2 = v2[-1] if v2.ndim == 1 else v2[-1]
        print(f"  파일1 마지막 프레임: {last1}")
        print(f"  파일2 마지막 프레임: {last2}")
        diff = np.array(last2, dtype=float) - np.array(last1, dtype=float)
        print(f"  차이 (파일2 - 파일1): {diff}")
    else:
        print(f"  파일1: {v1}")
        print(f"  파일2: {v2}")
        diff = float(v2) - float(v1)
        print(f"  차이: {diff}")

# 파일1의 끝 ~ 파일2의 시작 연속성 확인
print("\n" + "=" * 60)
print("[연속성 확인: 파일1 마지막 vs 파일2 첫 번째]")

for key in common_keys:
    if any(key.startswith(p) for p in skip_prefixes):
        continue

    v1 = data1[key]
    v2 = data2[key]

    if not np.issubdtype(v1.dtype, np.number):
        continue
    if v1.ndim < 1 or v2.ndim < 1:
        continue

    last1 = v1[-1]
    first2 = v2[0]
    diff = np.array(first2, dtype=float) - np.array(last1, dtype=float)
    print(f"\n[{key}]")
    print(f"  파일1[-1]: {last1}")
    print(f"  파일2[ 0]: {first2}")
    print(f"  차이: {diff}")
