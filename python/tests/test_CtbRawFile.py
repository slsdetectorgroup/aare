# SPDX-License-Identifier: MPL-2.0


import pytest
import numpy as np
from aare import CtbRawFile


@pytest.mark.withdata
def test_basic_properites(test_data_path):
    f = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')
    assert f.total_frames == 47
    assert f.image_size_in_bytes == 640
    assert f.master.analog_samples == 10
    assert f.master.digital_samples is None
    assert f.master.transceiver_samples is None


@pytest.mark.withdata
def test_seek_and_read(test_data_path):
    f = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')
    f.seek(0)
    header, raw_data = f.read_frame()
    assert header['frameNumber'] == 48 #we know that the first frame in this file is 48

    #Seek a multiple of frames in file
    f.seek(9)
    header, raw_data = f.read_frame()
    assert header['frameNumber'] == 57 

@pytest.mark.withdata
def test_read_all_frames_direct_and_iterate(test_data_path):
    """
    We want to make sure that iterating and reading give the same result.
    """
    f0 = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')
    f1 = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')

    for h0, d0 in f0:
        h1, d1 = f1.read_frame()
        assert h0 == h1
        assert np.all(d0 == d1)

@pytest.mark.withdata
def test_read_all_frames_with_index_and_iterate(test_data_path):
    """
    We want to make sure that iterating and reading give the same result.
    """
    f0 = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')
    f1 = CtbRawFile(test_data_path/'raw/ctb/run_master_0.json')

    frames_read = 0
    for i,(h0, d0) in enumerate(f0):
        h1, d1 = f1.read_frame(i)
        assert h0 == h1
        assert np.all(d0 == d1)
        frames_read += 1

    assert frames_read == f0.total_frames
