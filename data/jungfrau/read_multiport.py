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

frames = 10
parts = 2
frame_per_file = 3
bytes_per_pixel = 2
frame_cols = 1024
frame_rows = 512

part_cols = 1024
part_rows = 256


parts_data = np.zeros((frames,parts,part_rows,part_cols), dtype = np.uint16)
data = np.zeros((frames,frame_rows,frame_cols), dtype = np.uint16)
header = np.zeros((frames,parts), dtype = header_dt)



# verify that all parts have the same frame number
for frame in range(frames):
    for part in range(parts):
        file_name = f'jungfrau_double_d{part}_f{frame//frame_per_file}_{0}.raw'
        with open(file_name) as f:
            offset = (frame%frame_per_file)*(header_dt.itemsize+part_rows*part_cols*bytes_per_pixel)
            # print(f"Reading file: {file_name} at offset {offset}")
            header[frame,part] = np.fromfile(f, dtype=header_dt, count = 1,offset=offset)
            # print(f"Frame {frame} part {part} frame number: {header[frame,part]['Frame Number']}")
            if part > 0:
                assert header[frame,part]['Frame Number'] == header[frame,0]['Frame Number']

print("[X] All parts have the same frame number\n")


for frame in range(frames):

    for part in range(parts):
        file_name = f'jungfrau_double_d{part}_f{frame//frame_per_file}_{0}.raw'
        # print("Reading file:", file_name)
        with open(file_name) as f:
            offset = (frame%frame_per_file)*(header_dt.itemsize+part_rows*part_cols*bytes_per_pixel)
            header[frame,part] = np.fromfile(f, dtype=header_dt, count = 1, offset=offset)
            parts_data[frame,part] = np.fromfile(f, dtype=np.uint16,count = part_rows*part_cols).reshape(part_rows,part_cols)

    
    data[frame] = np.concatenate((parts_data[frame,0],parts_data[frame,1]),axis=0)



pixel_0_0,pixel_0_1,pixel_1_0,pixel_255_1023,pixel_511_1023,= [],[],[],[],[]
for frame in range(frames):
    pixel_0_0.append(data[frame,0,0])
    pixel_0_1.append(data[frame,0,1])
    pixel_1_0.append(data[frame,1,0])
    pixel_255_1023.append(data[frame,255,1023])
    pixel_511_1023.append(data[frame,511,1023])
print("upper left corner of each frame (pixel_0_0)")
print(pixel_0_0)
print("first pixel on new line of each frame (pixel_1_0)")
print(pixel_1_0)
print("second pixel of the first line of each frame (pixel_0_1)")
print(pixel_0_1)
print("first pixel of the second part on the last line of each frame (pixel_255_1023)")
print(pixel_255_1023)
print("lower right corner of each frame (pixel_511_1023)")
print(pixel_511_1023)
