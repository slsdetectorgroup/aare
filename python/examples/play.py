import matplotlib.pyplot as plt
import numpy as np
plt.ion()

import aare
from aare import CtbRawFile
print('aare imported')
from aare import transform
print('transform imported')
from pathlib import Path

import json

def decode(frames, rawdata):
    # rawdata = np.fromfile(f, dtype = np.uint16)
    counters = int((np.shape(rawdata)[0]/frames-56)/(48*48))
    print('Counters:', counters)
    rawdata = rawdata.reshape(frames,-1)[:,56:]
    rawdata = rawdata.reshape(frames,576*counters,4) #Data come in "blocks" of 4 pixels/receiver
    tr1 = rawdata[:,0:576*counters:2] #Transceiver1
    tr1=tr1.reshape((frames,48*counters,24)) 

    tr2 = rawdata[:,1:576*counters:2] #Transceiver2
    tr2=tr2.reshape((frames,48*counters,24))     
    
    data = np.append(tr1,tr2,axis=2)
    return data

def get_Mh02_frames(fname):
    # this function gives you the data from a file that is not a scan
    # it returns a (frames,48*counters,48)

    jsonf = open(fname)
    jsonpar = json.load(jsonf)
    jsonf.close()

    frames=jsonpar["Frames in File"]
    print('Frames:', frames)

    rawf = fname.replace('master','d0_f0')	
    rawf = rawf.replace('.json','.raw')	

    with open(rawf, 'rb') as f:
        rawdata = np.fromfile(f, dtype = np.uint16)
        data = decode(frames, rawdata) 
        print('Data:', np.shape(data))
        
        return data


#target format
# [frame, counter, row, col]
# plt.imshow(data[0,0])


# p = Path('/Users/erik/data/aare_test_data/jungfrau/jungfrau_single_master_0.json')

# f = aare.File(p)
# frame = f.read_frame()

# fig, ax = plt.subplots()
# im = ax.imshow(frame, cmap='viridis')


# fpath = Path('/Users/erik/data/Moench03old/test_034_irradiated_noise_g4_hg_exptime_2000us_master_0.json')
# # fpath = Path('/Users/erik/data/Moench05/moench05_multifile_master_0.json')


# # f = aare.CtbRawFile(fpath, transform = transform.moench05)
# # with CtbRawFile(fpath, transform = transform.moench05) as f:
# #     for header, image in f:
# #         print(f'Frame number: {header["frameNumber"]}')

# # m = aare.RawMasterFile(fpath)
# f = aare.File(fpath)



# cf = aare.ClusterFinder((400,400),(3,3))

# for i in range(100):
#     cf.push_pedestal_frame(f.read_frame())


# f.seek(0)
# pd = f.read_n(100).mean(axis=0)

# clusters = cf.find_clusters_without_threshold(f.read_frame())



# base = Path('/Users/erik/data/matterhorn/raw')
# fpath = Path(base / 'scan_15keV_vrf700_vrsh700_th0_master_0.json')
# f = aare.CtbRawFile(fpath, transform=transform.matterhorn02)
# f.seek(100)
# header1, image1 = f.read_frame()

# fpath = Path(base / 'scan_all15keV_vrf500_vrsh700_th0_master_0.json')

# f = aare.CtbRawFile(fpath, transform=transform.matterhorn02)
# f.seek(100)
# header4, image4 = f.read_frame()

# n_counters = image.shape[1] / 48**2 / 2

# for i in range(100):
#     header, image = f.read_frame()
#     print(header['frameNumber'])





#Data come in "blocks" of 4 pixels/receiver
# data = get_Mh02_frames(fpath.as_posix())

# rawi = np.zeros(48*48*4+56, dtype = np.uint16)
# for i,v in enumerate(rawi[56:]):
#     rawi[i+56] = i

# raw = image.view(np.uint16)

# pixel_map = decode(1, rawi)
# # img = np.take(raw, pixel_map)

# pm = np.zeros((4, 48,48), dtype = np.int64)
# for counter in range(4):
#     for row in range(48):
#         for col in range(48):
#             pm[counter, row, col] = row*48 + col+counter*48*48


# f2 = aare.CtbRawFile(fpath, transform=transform.matterhorn02)
# header, data = f2.read()
# plt.plot(data[:,0,20,20])
from aare import RawMasterFile, File
fpath = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/Jungfrau10/Jungfrau_DoubleModule_1UDP_ROI/SideBySide/241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_master_0.json')
m = RawMasterFile(fpath)
f = File(fpath)