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


base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/ci/aare_test_data')
# p = Path(base / 'jungfrau/jungfrau_single_master_0.json')

# f = aare.File(p)
# for i in range(10):
#     frame = f.read_frame()



# # f2 = aare.CtbRawFile(fpath, transform=transform.matterhorn02)
# # header, data = f2.read()
# # plt.plot(data[:,0,20,20])
# from aare import RawMasterFile, File, RawSubFile, DetectorType, RawFile
# base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/Jungfrau10/Jungfrau_DoubleModule_1UDP_ROI/SideBySide/')
# fpath = Path('241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_master_0.json')
# raw = Path('241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_d0_f0_0.raw')



# m = RawMasterFile(base / fpath)
# # roi = m.roi
# # rows = roi.ymax-roi.ymin+1
# # cols = 1024-roi.xmin
# # sf = RawSubFile(base / raw, DetectorType.Jungfrau, rows, cols, 16)



from aare import RawFile, File

base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/Jungfrau10/Jungfrau_DoubleModule_1UDP_ROI/')
fname = base / Path('SideBySide/241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_master_0.json')
# fname = Path(base / 'jungfrau/jungfrau_single_master_0.json')
# fname = base / 'Stacked/241024_JF10_m450_m367_KnifeEdge_TestBesom_9keV_750umFilter_PedestalStart_ZPos_-6_master_0.json'

f = RawFile(fname)
h,img = f.read_frame()
print(f'{h["frameNumber"]}')
