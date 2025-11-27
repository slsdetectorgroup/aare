
import numpy as np


n_counters = 64*3
bitfield_size = 64

header_dt = [('frameNumber',np.uint64),
             ('expLength',np.uint32),
             ('packetNumber', np.uint32),
             ('bunchId', np.uint64),
             ('timestamp', np.uint64),
             ('modId', np.uint16),
             ('row', np.uint16),
             ('col', np.uint16),
             ('reserved', np.uint16),
             ('debug', np.uint32),
             ('roundRNumber', np.uint16),
             ('detType', np.uint8),
             ('version', np.uint8)]




def ExtractBits(raw_data, dr=24, bits = (17,6)):
    bits = np.uint64(bits)
    data = np.zeros(0, dtype = np.uint64)
    for bit in bits:
        tmp = (raw_data >> bit) & np.uint64(1)
        data = np.hstack((data, tmp))
    
    #Shift the bits to the righ place
    for i in np.arange(dr, dtype = np.uint64):
        data[i::dr] = data[i::dr] << i

    data = data.reshape(data.size//dr, dr)
    return data.sum(axis = 1)




def read_my302_file(fname, dr=24, bits = (17,6),
                    offset=48, tail = 72, n_frames=1):
    header = np.zeros(n_frames, header_dt)
    data = np.zeros((n_frames, n_counters), dtype = np.uint64)
    with open(fname, 'rb') as f:
        for i in range(n_frames):
            header[i], raw_data = _read_my302_frame(f, offset, tail, dr)
            data[i] = ExtractBits(raw_data, dr=dr, bits = bits)
    return header, data


def _read_my302_frame(f, offset, tail, dr):
    header = np.fromfile(f, count=1, dtype = header_dt)
    f.seek(bitfield_size+offset, 1)
    data = np.fromfile(f, count = int(n_counters*dr/2), dtype = np.uint64)
    f.seek(tail, 1)
    return header, data
