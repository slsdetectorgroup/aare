import numpy as np
from pathlib import Path

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



with open("data/eiger/eiger_500k_16bit_d0_f0_0.raw", "rb") as f:
    for i in range(3):
        frame_number = np.fromfile(f, dtype=header_dt, count=1)["Frame Number"][0]
        print(frame_number)
        f.seek(262144,1)
