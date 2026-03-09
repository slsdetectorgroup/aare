import pytest 
import numpy as np
from aare import CtbRawFile, transform

@pytest.mark.withdata
def test_matterhorn10_16bit(test_data_path): 
    """Matterhorn10Transform 1 counter 16 bit dynamic range"""
    with CtbRawFile(test_data_path / "raw/Matterhorn10/16bit_master_0.json", transform = transform.Matterhorn10Transform(dynamic_range=16, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (256, 256)
        assert frames.dtype == np.uint16

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint16), (256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)


@pytest.mark.withdata
def test_matterhorn10_8bit(test_data_path): 
    """Matterhorn10Transform 1 counter 8 bit dynamic range"""
    with CtbRawFile(test_data_path / "raw/Matterhorn10/8bit_master_1.json", transform = transform.Matterhorn10Transform(dynamic_range=8, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (256, 256)
        assert frames.dtype == np.uint8

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint8), (256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)


@pytest.mark.withdata
def test_matterhorn10_4bit(test_data_path): 
    """ Matterhorn10Transform 1 counter 4 bit dynamic range """
    with CtbRawFile(test_data_path / "raw/Matterhorn10/newnewrun_4bit_1counter_master_0.json", transform = transform.Matterhorn10Transform(dynamic_range=4, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (256, 256)
        assert frames.dtype == np.uint8

        expected_data = np.tile(np.tile(np.arange(15, -1, -1, dtype=np.uint8), 16), (256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)

@pytest.mark.withdata
def test_matterhorn10_16bit_4counters(test_data_path): 
    """Matterhorn10Transform 4 counters 16 bit dynamic range"""

    with CtbRawFile(test_data_path / "raw/Matterhorn10/4counter_16bit_master_4.json", transform = transform.Matterhorn10Transform(dynamic_range=16, num_counters=4)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (4*256, 256)
        assert frames.dtype == np.uint16

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint16), (4*256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)
