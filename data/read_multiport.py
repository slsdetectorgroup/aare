import numpy as np
import matplotlib.pyplot as plt
plt.ion()

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
parts = 2

frame_cols = 1024
frame_rows = 512

part_cols = 1024
part_rows = 256


parts_data = np.zeros((frames,parts,part_rows,part_cols), dtype = np.uint16)
data = np.zeros((frames,frame_rows,frame_cols), dtype = np.uint16)
header = np.zeros((frames,parts), dtype = header_dt)




for frame in range(frames):

    for part in range(parts):
        file_name = f'jungfrau_double_d{part}_f{frame}_{0}.raw'
        print("Reading file:", file_name)
        with open(file_name) as f:
            header[frame,part] = np.fromfile(f, dtype=header_dt, count = 1)
            parts_data[frame,part] = np.fromfile(f, dtype=np.uint16,count = part_rows*part_cols).reshape(part_rows,part_cols)

    
    data[frame] = np.concatenate((parts_data[frame,0],parts_data[frame,1]),axis=0)



# for frame in range(frames):
#     print("Frame:", frame)
#     print("Data:\n", data[frame])

# print(data[0,0,0])
# print(data[0,0,1])
# print(data[0,0,50])
print(data[0,0,0])
print(data[0,0,1])
print(data[0,255,1023])

print(data[0,511,1023])
# print()
# print(parts_data[0,0,0,0])
# print(parts_data[0,0,0,1])
# print(parts_data[0,0,1,0])

# print(data.shape)



#fig, ax = plt.subplots()
#im = ax.imshow(data[0])
#im.set_clim(2000,4000)
