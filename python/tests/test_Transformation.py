# SPDX-License-Identifier: MPL-2.0
import pytest 
import numpy as np
from aare import CtbRawFile, transform

@pytest.mark.withdata
def test_matterhorn10_16bit(test_data_path): 
    """Matterhorn10Transform 1 counter 16 bit dynamic range"""
    with CtbRawFile(test_data_path / "raw/Matterhorn10/16bit_master_0.json", transform = transform.Matterhorn10Transform(dynamic_range=16, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (1, 256, 256)
        assert frames.dtype == np.uint16

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint16), (1, 256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)


@pytest.mark.withdata
def test_matterhorn10_8bit(test_data_path): 
    """Matterhorn10Transform 1 counter 8 bit dynamic range"""
    with CtbRawFile(test_data_path / "raw/Matterhorn10/8bit_master_1.json", transform = transform.Matterhorn10Transform(dynamic_range=8, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (1, 256, 256)
        assert frames.dtype == np.uint8

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint8), (1, 256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)

@pytest.mark.withdata
def test_matterhorn10_4bit(test_data_path): 
    """ Matterhorn10Transform 1 counter 4 bit dynamic range """
    with CtbRawFile(test_data_path / "raw/Matterhorn10/newnewrun_4bit_1counter_master_0.json", transform = transform.Matterhorn10Transform(dynamic_range=4, num_counters=1)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (1, 256, 256)
        assert frames.dtype == np.uint8

        expected_data = np.tile(np.tile(np.arange(15, -1, -1, dtype=np.uint8), 16), (1, 256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)

@pytest.mark.withdata
def test_matterhorn10_16bit_4counters(test_data_path): 
    """Matterhorn10Transform 4 counters 16 bit dynamic range"""

    with CtbRawFile(test_data_path / "raw/Matterhorn10/4counter_16bit_master_4.json", transform = transform.Matterhorn10Transform(dynamic_range=16, num_counters=4)) as f:
        headers, frames = f.read_frame()

        assert frames.shape == (4, 256, 256)
        assert frames.dtype == np.uint16

        expected_data = np.tile(np.arange(255, -1, -1,dtype=np.uint16), (4, 256, 1)) # TODO: endianess issue ? 

        assert np.all(frames == expected_data)


def test_matterhorn10_4bit_accepts_2d_frame():
    """CtbRawFile.read_frame returns a (1, n_bytes) array; the 4 bit path must accept it"""
    n_bytes = (transform.Matterhorn10.nRows * transform.Matterhorn10.nCols * 4) // 8
    data = (np.arange(n_bytes) % 256).astype(np.uint8).reshape(1, n_bytes)
    t = transform.Matterhorn10Transform(dynamic_range=4, num_counters=1)

    frames = t(data)

    assert frames.shape == (1, 256, 256)
    assert frames.dtype == np.uint8

    flat = data.reshape(-1)
    nibbles = np.stack([flat & 0x0F, flat >> 4], axis=-1).reshape(-1)
    expected = np.take(nibbles, t.pixel_map).reshape(1, 256, 256)
    assert np.array_equal(frames, expected)


def test_mythen302_accepts_2d_frame():
    """CtbRawFile.read_frame returns a (1, n_bytes) array; Mythen302Transform must accept it"""
    n_bytes = 64 * 3 * 3 + 2  # two extra bytes for the 4 bit offset
    rng = np.random.default_rng(0)
    data = rng.integers(0, 256, size=(1, n_bytes), dtype=np.uint8)
    t = transform.Mythen302Transform(offset=4)

    res = t(data)

    assert res.shape == (64, 3)
    assert res.dtype == np.uint32
    assert np.array_equal(res, t(data.reshape(-1)))
