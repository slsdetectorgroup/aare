import numpy as np

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
sync = True
for part in range(parts):
    file_name = f'CORRECTED_scan242_d{part}_f{frame//frame_per_file}_{3}.raw'
    with open(file_name) as f:
        offset = (frame%frame_per_file)*(header_dt.itemsize+part_rows*part_cols*bytes_per_pixel)
        # print(f"Reading file: {file_name} at offset {offset}")
        header[frame,part] = np.fromfile(f, dtype=header_dt, count = 1,offset=offset)
        # print(f"Frame {frame} part {part} frame number: {header[frame,part]['Frame Number']}")
        print(f"part {part} frame number: {header[frame,part]['Frame Number']}")
        sync  = sync and (header[frame,part]['Frame Number'] == header[frame,0]['Frame Number'])


if sync:
    print("[X] subfiles have the same frame")
else:
    print("[X] subfiles do not have the same frame\n")


