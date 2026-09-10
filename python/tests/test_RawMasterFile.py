

import json

import pytest
from aare import RawMasterFile, ReadoutMode, DetectorType


@pytest.mark.withdata
def test_read_rawfile_quad_eiger_and_compare_to_numpy(test_data_path): 
    
    file_name = test_data_path/'raw/jungfrau/jungfrau_single_master_0.json'
   
    f = RawMasterFile(file_name)
    assert(f.reading_mode == ReadoutMode.UNKNOWN)
    assert(f.detector_type == DetectorType.Jungfrau)


def test_raw_master_file_context_manager(tmp_path):
    file_name = tmp_path / "run_master_0.json"
    file_name.write_text(json.dumps({
        "Version": 7.2,
        "Detector Type": "Jungfrau",
        "Timing Mode": "auto",
        "Geometry": {"x": 1, "y": 1},
        "Image Size in bytes": 12,
        "Pixels": {"x": 3, "y": 2},
        "Max Frames Per File": 1,
        "Total Frames": 2,
        "Frames in File": 2,
        "Frame Padding": 1,
        "Frame Discard Policy": "nodiscard",
    }))

    
    with RawMasterFile(file_name) as context_file:
        assert context_file.reading_mode == ReadoutMode.UNKNOWN
        assert context_file.detector_type == DetectorType.Jungfrau
