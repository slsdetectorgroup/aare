import numpy as np
import shutil

header_dt = np.dtype(
    [
        ("Frame Number", "u8"),
        ("SubFrame Number/ExpLength", "u4"),
        ("Packet Number", "u4"),
        ("Bunch ID", "u8"),
        ("Timestamp", "u8"),
        ("Module Id", "u2"),
        ("Row", "u2"),
        ("Column", "u2"),
        ("Reserved", "u2"),
        ("Debug", "u4"),
        ("Round Robin Number", "u2"),
        ("Detector Type", "u1"),
        ("Header Version", "u1"),
        ("Packets caught mask", "8u8")
    ]
)

# Read three frames from a jungfrau file with a single interface

frames = 1
parts = 4
frame_per_file = 3
bytes_per_pixel = 4
frame_cols = 1
frame_rows = 5120

part_cols = 1280
part_rows = 1

header = np.zeros((frames,parts), dtype = header_dt)



# verify that all parts have the same frame number
frame = 0
i = 55
for part in range(parts):
    file_name_r = f'scan242_d{part}_f{frame//frame_per_file}_{3}.raw'
    file_name_w = f'CORRECTED_scan242_d{part}_f{frame//frame_per_file}_{3}.raw'
    shutil.copyfile(file_name_r, file_name_w)

    with open(file_name_r) as fr, open(file_name_w, 'r+b') as fw:
        # get frame
        offset = (frame%frame_per_file)*(header_dt.itemsize+part_rows*part_cols*bytes_per_pixel)
        header[frame,part] = np.fromfile(fr, dtype=header_dt, count = 1,offset=offset)
        # update frame number
        header[frame,part]['Frame Number'] = i
        fw.seek(offset)
        header[frame,part].tofile(fw)


print("[X] Done\n")

