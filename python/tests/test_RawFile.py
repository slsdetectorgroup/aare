import pytest
from aare import RawFile
import numpy as np

@pytest.mark.withdata
def test_read_rawfile_with_roi(test_data_path):

   with RawFile(test_data_path / "raw/SingleChipROI/Data_master_0.json") as f:
    headers, frames = f.read()

    assert headers.size == 10100
    assert frames.shape == (10100, 256, 256)
    
@pytest.mark.withdata
def test_read_rawfile_quad_eiger_and_compare_to_numpy(test_data_path): 
    
    d0 = test_data_path/'raw/eiger_quad_data/W13_vrpreampscan_m21C_300V_800eV_vthre2000_d0_f0_0.raw'
    d1 = test_data_path/'raw/eiger_quad_data/W13_vrpreampscan_m21C_300V_800eV_vthre2000_d1_f0_0.raw'

    image = np.zeros((512,512), dtype=np.uint32)

    with open(d0) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset = 20*256*512*4 + 112*21).reshape(256,512)

        image[256:,:] = raw

    with open(d1) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset = 20*256*512*4 + 112*21).reshape(256,512)
        
        image[0:256,:] = raw[::-1,:]

    with RawFile(test_data_path/'raw/eiger_quad_data/W13_vrpreampscan_m21C_300V_800eV_vthre2000_master_0.json') as f:
        f.seek(20)
        header, image1 = f.read_frame()
    
    assert (image == image1).all()


@pytest.mark.withdata
def test_read_rawfile_eiger_and_compare_to_numpy(test_data_path): 
    d0 = test_data_path/'raw/eiger/Lab6_20500eV_2deg_20240629_d0_f0_7.raw'
    d1 = test_data_path/'raw/eiger/Lab6_20500eV_2deg_20240629_d1_f0_7.raw'
    d2 = test_data_path/'raw/eiger/Lab6_20500eV_2deg_20240629_d2_f0_7.raw'
    d3 = test_data_path/'raw/eiger/Lab6_20500eV_2deg_20240629_d3_f0_7.raw'

    image = np.zeros((512,1024), dtype=np.uint32)

    #TODO why is there no header offset?
    with open(d0) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset=112).reshape(256,512)

        image[0:256,0:512] = raw[::-1]

    with open(d1) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset=112).reshape(256,512)
        
        image[0:256,512:] = raw[::-1]

    with open(d2) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset=112).reshape(256,512)
        
        image[256:,0:512] = raw

    with open(d3) as f:
        raw = np.fromfile(f, dtype=np.uint32, count = 256*512, offset=112).reshape(256,512)
        
        image[256:,512:] = raw

    
    with RawFile(test_data_path/'raw/eiger/Lab6_20500eV_2deg_20240629_master_7.json') as f:
        header, image1 = f.read_frame()

    assert (image == image1).all()
