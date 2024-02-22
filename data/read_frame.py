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
rows = 512
cols = 1024
frames = 3

data = np.zeros((frames,rows,cols), dtype = np.uint16)
header = np.zeros(frames, dtype = header_dt)
with open('jungfrau_single_d0_f0_0.raw') as f:
    for i in range(frames):
        header[i] = np.fromfile(f, dtype=header_dt, count = 1)
        data[i] = np.fromfile(f, dtype=np.uint16,count = rows*cols).reshape(rows,cols)

fig, ax = plt.subplots()
im = ax.imshow(data[0])
im.set_clim(2000,4000)
