###
### Verify that the raw file written by the raw_example.cpp are correct
###


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

frames = 1
parts = 1
frame_per_file = 3
bytes_per_pixel = 2
frame_cols = 512
frame_rows = 1024

part_cols = 512
part_rows = 1024


# parts_data = np.zeros((frames,parts,part_rows,part_cols), dtype = np.uint16)
data = np.zeros((frames,frame_rows,frame_cols), dtype = np.uint16)
header = np.zeros((frames,parts), dtype = header_dt)



# verify that all parts have the same frame number
for frame in range(frames):
    for part in range(parts):
        file_name = f'/tmp/raw_example_writing_d{part}_f{frame//frame_per_file}_{0}.raw'
        with open(file_name) as f:
            offset = (frame%frame_per_file)*(header_dt.itemsize+part_rows*part_cols*bytes_per_pixel)
            # print(f"Reading file: {file_name} at offset {offset}")
            header[frame,part] = np.fromfile(f, dtype=header_dt, count = 1,offset=offset)
            # print(f"Frame {frame} part {part} frame number: {header[frame,part]['Frame Number']}")
            data[frame] = np.fromfile(f, dtype=np.uint16,count = frame_rows*frame_cols).reshape(frame_rows,frame_cols)
            

for frame in range(frames):
    for i,j in np.ndindex(data[frame].shape):
        assert(data[frame][i,j] == i+j)

print("[X] frame data is correct")
        
            
