import pytest
import numpy as np
from aare import RawSubFile, DetectorType


@pytest.mark.files
def test_read_a_jungfrau_RawSubFile(test_data_path):
    with RawSubFile(test_data_path / "raw/jungfrau/jungfrau_single_d0_f1_0.raw", DetectorType.Jungfrau, 512, 1024, 16) as f:
        assert f.frames_in_file == 3

        headers, frames = f.read()

    assert headers.size == 3
    assert frames.shape == (3, 512, 1024)
    
    # Frame numbers in this file should be 4, 5, 6
    for i,h in zip(range(4,7,1), headers):
        assert h["frameNumber"] == i

    # Compare to canned data using numpy
    data = np.load(test_data_path / "raw/jungfrau/jungfrau_single_0.npy")
    assert np.all(data[3:6] == frames)

@pytest.mark.files
def test_iterate_over_a_jungfrau_RawSubFile(test_data_path):

    data = np.load(test_data_path / "raw/jungfrau/jungfrau_single_0.npy")

    with RawSubFile(test_data_path / "raw/jungfrau/jungfrau_single_d0_f0_0.raw", DetectorType.Jungfrau, 512, 1024, 16) as f:
        i = 0
        for header, frame in f:
            assert header["frameNumber"] == i+1
            assert np.all(frame == data[i])
            i += 1
        assert i == 3
        assert header["frameNumber"] == 3