import pytest
from aare import RawFile

@pytest.mark.files
def test_read_rawfile_with_roi(test_data_path):

    # Starting with f1 there is now 7 frames left in the series of files
    print(test_data_path)
    with RawFile(test_data_path / "raw/SingleChipROI/Data_master_0.json") as f:
        headers, frames = f.read()

    assert headers.size == 10100
    assert frames.shape == (10100, 256, 256)
    