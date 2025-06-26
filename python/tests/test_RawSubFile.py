import pytest
import numpy as np
from aare import RawSubFile, DetectorType


@pytest.mark.withdata
def test_read_a_jungfrau_RawSubFile(test_data_path):

    # Starting with f1 there is now 7 frames left in the series of files
    with RawSubFile(test_data_path / "raw/jungfrau/jungfrau_single_d0_f1_0.raw", DetectorType.Jungfrau, 512, 1024, 16) as f:
        assert f.frames_in_file == 7

        headers, frames = f.read()

    assert headers.size == 7
    assert frames.shape == (7, 512, 1024)
    

    for i,h in zip(range(4,11,1), headers):
        assert h["frameNumber"] == i

    # Compare to canned data using numpy
    data = np.load(test_data_path / "raw/jungfrau/jungfrau_single_0.npy")
    assert np.all(data[3:] == frames)

@pytest.mark.withdata
def test_iterate_over_a_jungfrau_RawSubFile(test_data_path):

    data = np.load(test_data_path / "raw/jungfrau/jungfrau_single_0.npy")

    # Given the first subfile in a series we can read all frames from f0, f1, f2...fN
    with RawSubFile(test_data_path / "raw/jungfrau/jungfrau_single_d0_f0_0.raw", DetectorType.Jungfrau, 512, 1024, 16) as f:
        i = 0
        for header, frame in f:
            assert header["frameNumber"] == i+1
            assert np.all(frame == data[i])
            i += 1
        assert i == 10
        assert header["frameNumber"] == 10
