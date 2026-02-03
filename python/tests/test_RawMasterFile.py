

import pytest
from aare import RawMasterFile, ReadingMode, DetectorType


@pytest.mark.withdata
def test_read_rawfile_quad_eiger_and_compare_to_numpy(test_data_path): 
    
    file_name = test_data_path/'raw/jungfrau/jungfrau_single_master_0.json'
   
    f = RawMasterFile(file_name)
    assert(f.reading_mode == ReadingMode.Unknown)
    assert(f.detector_type == DetectorType.Jungfrau)