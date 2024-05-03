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
rows = 1024
cols = 512
frames = 1

data = np.zeros((frames,rows,cols), dtype = np.uint16)
header = np.zeros(frames, dtype = header_dt)
for frame in range(frames):
    
    file_name = '/tmp/raw_example_writing_master_'
    print("Reading file:", file_name)
    with open(file_name) as f:
        for i in range(3 if file_id != 3 else 1):
            header[i+file_id*3] = np.fromfile(f, dtype=header_dt, count = 1)
            data[i+file_id*3] = np.fromfile(f, dtype=np.uint16,count = rows*cols).reshape(rows,cols)


    # for i in range(frames if file_id != 3 else 1 ):
    #     print("frame:",i)
    #     print(header[i][0,0],data[i][0,1],data[i][1,0],data[i][rows-1,cols-1])
    #     print("")


print(header[1]["Frame Number"])
#fig, ax = plt.subplots()
#im = ax.imshow(data[0])
#im.set_clim(2000,4000)
