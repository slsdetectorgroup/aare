import pytest
import numpy as np
from aare import JungfrauDataFile

@pytest.mark.files
def test_jfungfrau_dat_read_number_of_frames(test_data_path):
    with JungfrauDataFile(test_data_path / "dat/AldoJF500k_000000.dat") as dat_file:
        assert dat_file.total_frames == 24

    with JungfrauDataFile(test_data_path / "dat/AldoJF250k_000000.dat") as dat_file:
        assert dat_file.total_frames == 53
    
    with JungfrauDataFile(test_data_path / "dat/AldoJF65k_000000.dat") as dat_file:
        assert dat_file.total_frames == 113


@pytest.mark.files
def test_jfungfrau_dat_read_number_of_file(test_data_path):
    with JungfrauDataFile(test_data_path / "dat/AldoJF500k_000000.dat") as dat_file:
        assert dat_file.n_files == 4

    with JungfrauDataFile(test_data_path / "dat/AldoJF250k_000000.dat") as dat_file:
        assert dat_file.n_files == 7
    
    with JungfrauDataFile(test_data_path / "dat/AldoJF65k_000000.dat") as dat_file:
        assert dat_file.n_files == 7


@pytest.mark.files
def test_read_module(test_data_path):
    """
    Read all frames from the series of .dat files. Compare to canned data in npz format. 
    """

    # Read all frames from the .dat file
    with JungfrauDataFile(test_data_path / "dat/AldoJF500k_000000.dat") as f:
        header, data = f.read()

    #Sanity check
    n_frames = 24
    assert header.size == n_frames
    assert data.shape == (n_frames, 512, 1024)

    # Read reference data using numpy
    with np.load(test_data_path / "dat/AldoJF500k.npz") as f:
        ref_header = f["headers"]
        ref_data = f["frames"]

    # Check that the data is the same
    assert np.all(ref_header == header)
    assert np.all(ref_data == data)

@pytest.mark.files
def test_read_half_module(test_data_path):

    # Read all frames from the .dat file
    with JungfrauDataFile(test_data_path / "dat/AldoJF250k_000000.dat") as f:
        header, data = f.read()

    n_frames = 53
    assert header.size == n_frames
    assert data.shape == (n_frames, 256, 1024)

    # Read reference data using numpy
    with np.load(test_data_path / "dat/AldoJF250k.npz") as f:
        ref_header = f["headers"]
        ref_data = f["frames"]

    # Check that the data is the same
    assert np.all(ref_header == header)
    assert np.all(ref_data == data)


@pytest.mark.files
def test_read_single_chip(test_data_path):

    # Read all frames from the .dat file
    with JungfrauDataFile(test_data_path / "dat/AldoJF65k_000000.dat") as f:
        header, data = f.read()

    n_frames = 113
    assert header.size == n_frames
    assert data.shape == (n_frames, 256, 256)

    # Read reference data using numpy
    with np.load(test_data_path / "dat/AldoJF65k.npz") as f:
        ref_header = f["headers"]
        ref_data = f["frames"]

    # Check that the data is the same
    assert np.all(ref_header == header)
    assert np.all(ref_data == data)