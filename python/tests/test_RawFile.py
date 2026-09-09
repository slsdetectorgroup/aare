# SPDX-License-Identifier: MPL-2.0
import pytest
import json
from aare import File, RawFile, RawSubFile, DetectorType, ROI, UDPPortPosition
import numpy as np


@pytest.fixture
def small_raw_file(tmp_path):
    master_path = tmp_path / "run_master_0.json"
    master_path.write_text(json.dumps({
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
    for index in range(2):
        data_path = tmp_path / f"run_d0_f{index}_0.raw"
        # A detector header followed by six uint16 pixels.
        data_path.write_bytes(bytes(112) + np.arange(6, dtype=np.uint16).tobytes())
    return master_path


@pytest.mark.parametrize("method, args, kwargs", [
    ("read_frame", (), {}),
    ("read_n", (1,), {}),
    ("read_roi", (0,), {}),
    ("read_rois", (), {}),
    ("read_n_with_roi", (1,), {"roi_index": 0}),
    ("frame_number", (2,), {}),
])
def test_raw_read_error_context(small_raw_file, method, args, kwargs):
    reader = RawFile(small_raw_file)
    reader.seek(2)
    with pytest.raises(RuntimeError) as error:
        getattr(reader, method)(*args, **kwargs)
    assert "frame index 2" in str(error.value)
    assert str(small_raw_file) in str(error.value)
    assert ".raw" not in str(error.value)
    if method == "read_frame":
        assert "file contains 2 frames (indices are zero-based)" in str(error.value)


@pytest.mark.parametrize("frame_index", [2, 17])
def test_generic_raw_read_error_context(small_raw_file, frame_index):
    reader = File(small_raw_file)
    with pytest.raises(RuntimeError) as error:
        reader.read_frame(frame_index)
    assert f"frame index {frame_index}" in str(error.value)
    assert str(small_raw_file) in str(error.value)


@pytest.mark.parametrize("raw_subfile", [False, True])
def test_truncated_raw_read_error_context(small_raw_file, raw_subfile):
    data_path = small_raw_file.parent / "run_d0_f1_0.raw"
    if raw_subfile:
        reader = RawSubFile(
            small_raw_file.parent / "run_d0_f0_0.raw",
            DetectorType.Jungfrau, 2, 3, 16,
        )
    else:
        reader = RawFile(small_raw_file)
    data_path.write_bytes(bytes(112))
    reader.seek(1)
    with pytest.raises(RuntimeError) as error:
        reader.read_frame()
    assert "frame index 1" in str(error.value)
    assert str(data_path) in str(error.value)
    assert "End of file" in str(error.value)
    assert "master" not in str(error.value)
    assert "RawSubFile.cpp" not in str(error.value)


@pytest.mark.parametrize("method, args", [
    ("read_frame", ()),
    ("read_n", (1,)),
    ("read_roi", (0,)),
    ("frame_number", (1,)),
])
def test_short_raw_subfile_sets_read_bounds(small_raw_file, method, args):
    (small_raw_file.parent / "run_d0_f1_0.raw").unlink()
    reader = RawFile(small_raw_file)
    reader.seek(1)
    with pytest.raises(RuntimeError) as error:
        getattr(reader, method)(*args)
    assert reader.total_frames == 1
    assert "frame index 1" in str(error.value)
    assert str(small_raw_file) in str(error.value)
    assert ".raw" not in str(error.value)


@pytest.mark.parametrize("counts, master_count, warn", [
    ([2], 2, False),
    ([2, 2], 2, False),
    ([2, 2], 0, True),
    ([2, 2], 1, True),
    ([2, 2], 5, True),
    ([1, 2], 2, True),
    ([2, 1], 2, True),
    ([1, 2], 1, True),
    ([0, 2], 2, True),
    ([0, 0], 0, False),
])
def test_raw_subfile_frame_counts(small_raw_file, capfd, counts, master_count, warn):
    metadata = json.loads(small_raw_file.read_text())
    metadata["Geometry"]["y"] = len(counts)
    metadata["Frames in File"] = master_count
    metadata["Total Frames"] = 1000
    small_raw_file.write_text(json.dumps(metadata))
    data = (small_raw_file.parent / "run_d0_f0_0.raw").read_bytes()
    for module, count in enumerate(counts):
        for index in range(2):
            path = small_raw_file.parent / f"run_d{module}_f{index}_0.raw"
            if index < count:
                path.write_bytes(data)
            elif index == 0:
                path.write_bytes(b"")
            else:
                path.unlink(missing_ok=True)

    reader = RawFile(small_raw_file)
    warning = capfd.readouterr().err
    if warn:
        assert warning.count("WARNING") == 1
        assert str(small_raw_file) in warning
        assert (
            f"min/max frame count in a subfile: {min(counts)}/{max(counts)}"
            in warning
        )
        assert f"master records {master_count}" in warning
        assert f"using {min(counts)} frames" in warning
    else:
        assert "WARNING" not in warning

    assert reader.total_frames == min(counts)
    assert len(reader) == min(counts)
    assert reader.master.frames_in_file == master_count
    if min(counts):
        _, frames = reader.read()
        assert frames.shape == (min(counts), 2 * len(counts), 3)
        reader.seek(0)
        _, frames = reader.read_n(10)
        assert len(frames) == min(counts)
        assert reader.frame_number(min(counts) - 1) == 0
    with pytest.raises(RuntimeError):
        reader.frame_number(min(counts))
    reader.seek(min(counts))
    with pytest.raises(RuntimeError):
        reader.read_frame()
    assert "WARNING" not in capfd.readouterr().err


@pytest.mark.parametrize("short_module", [0, 1])
def test_raw_frame_count_across_rois(small_raw_file, short_module):
    metadata = json.loads(small_raw_file.read_text())
    metadata.update({
        "Version": 8.0,
        "Geometry": {"x": 1, "y": 2},
        "Image Size": 12,
        "Receiver Rois": [
            {"xmin": 0, "xmax": 2, "ymin": 0, "ymax": 1},
            {"xmin": 0, "xmax": 2, "ymin": 2, "ymax": 3},
        ],
    })
    small_raw_file.write_text(json.dumps(metadata))
    for index in range(2):
        data = (small_raw_file.parent / f"run_d0_f{index}_0.raw").read_bytes()
        (small_raw_file.parent / f"run_d1_f{index}_0.raw").write_bytes(data)
    (small_raw_file.parent / f"run_d{short_module}_f1_0.raw").unlink()

    reader = RawFile(small_raw_file)
    assert reader.total_frames == 1
    _, rois = reader.read_rois()
    assert len(rois) == 2
    assert all(roi.shape == (2, 3) for roi in rois)
    for roi_index in range(2):
        reader.seek(0)
        _, frames = reader.read_n_with_roi(10, roi_index=roi_index)
        assert frames.shape == (1, 2, 3)
        with pytest.raises(RuntimeError):
            reader.read_roi(roi_index)


@pytest.mark.parametrize("disabled_port", [0, 1])
def test_raw_frame_count_with_disabled_udp_port(small_raw_file, disabled_port):
    metadata = json.loads(small_raw_file.read_text())
    metadata.update({
        "Geometry": {"x": 1, "y": 2},
        "Frames in File": 0,
        "Number of UDP Interfaces": 2,
        "UDP Ports Type": ["bottom", "top"],
        "UDP Ports Disabled": [disabled_port],
    })
    small_raw_file.write_text(json.dumps(metadata))
    data = (small_raw_file.parent / "run_d0_f0_0.raw").read_bytes()
    for index in range(2):
        (small_raw_file.parent / f"run_d0_f{index}_0.raw").unlink()
        path = small_raw_file.parent / f"run_d{1 - disabled_port}_f{index}_0.raw"
        path.write_bytes(data)

    reader = RawFile(small_raw_file)
    assert reader.total_frames == 2
    assert reader.master.frames_in_file == 0
    assert reader.frame_number(1) == 0
    _, frames = reader.read()
    assert frames.shape == (2, 2, 3)


def test_raw_frame_count_without_selected_subfiles(small_raw_file):
    metadata = json.loads(small_raw_file.read_text())
    metadata.update({"Version": 8.0, "Image Size": 12, "Receiver Rois": []})
    small_raw_file.write_text(json.dumps(metadata))
    with pytest.raises(RuntimeError, match="No raw subfiles selected"):
        RawFile(small_raw_file)


@pytest.mark.parametrize("lagging_module", [0, 1])
def test_raw_synchronization_error_identifies_subfile(
    small_raw_file, lagging_module
):
    metadata = json.loads(small_raw_file.read_text())
    metadata["Geometry"]["y"] = 2
    small_raw_file.write_text(json.dumps(metadata))
    for index in range(2):
        data = (small_raw_file.parent / f"run_d0_f{index}_0.raw").read_bytes()
        (small_raw_file.parent / f"run_d1_f{index}_0.raw").write_bytes(data)

    ahead_path = small_raw_file.parent / f"run_d{1 - lagging_module}_f1_0.raw"
    with ahead_path.open("r+b") as output:
        output.write(np.array([1], dtype=np.uint64).tobytes())

    reader = RawFile(small_raw_file)
    reader.seek(1)
    with pytest.raises(RuntimeError) as error:
        reader.read_frame()
    assert "frame index 1" in str(error.value)
    assert "synchroniz" in str(error.value)
    lagging_path = small_raw_file.parent / f"run_d{lagging_module}_f1_0.raw"
    assert str(lagging_path) in str(error.value)

@pytest.mark.withdata
def test_read_rawfile_with_roi_spanning_over_one_module(test_data_path):

    with RawFile(test_data_path / "raw/ROITestData/SingleChipROI/Data_master_0.json") as f:
        headers, frames = f.read()

        num_rois = f.num_rois
        assert num_rois == 1
        assert headers.size == 10100
        assert frames.shape == (10100, 256, 256)

        assert headers.size == 10100
        assert frames.shape == (10100, 256, 256) 


@pytest.mark.withdata
def test_read_rawfile_with_multiple_rois(test_data_path): 
    with RawFile(test_data_path / "raw/ROITestData/MultipleROIs/run_master_0.json") as f:
        num_rois = f.num_rois

        #cannot read_frame for multiple ROIs
        with pytest.raises(RuntimeError):
            f.read_frame()

        assert f.tell() == 0
        _, frames = f.read_rois() 
        assert num_rois == 2
        assert len(frames) == 2
        assert frames[0].shape == (301, 101)
        assert frames[1].shape == (101, 101)

        assert f.tell() == 1

        # read multiple ROIs at once
        _, frames = f.read_n_with_roi(2, roi_index = 1) 
        assert frames.shape == (2, 101, 101)

        assert f.tell() == 3

        f.seek(1)

        # read specific ROI
        _, frame = f.read_roi(0)
        assert frame.shape == (301, 101)
        assert f.tell() == 2
   
    

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


@pytest.mark.withdata 
def test_read_eiger_udp_port_disabled(test_data_path):
    with RawFile(test_data_path / "raw/eiger/one_udp_port_disabled_master_0.json") as f:
        _, frame = f.read_rois() 

        assert(len(frame) == 2)
        assert frame[0].shape == (256, 512)
        assert frame[1].shape == (256, 1024)
        assert f.master.udp_port_types == [UDPPortPosition.LEFT, UDPPortPosition.RIGHT]
        rois = f.master.rois
        assert len(rois) == 2
        assert rois[0] == ROI(512, 1024, 0, 256)
        assert rois[1] == ROI(0, 1024, 256, 512)

    with RawFile(test_data_path / "raw/eiger/quad_eiger_disabled_bottom_port_master_0.json") as f:
        _, frame = f.read_frame() 

        assert frame.shape == (256, 512)
        assert(f.master.disabled_udp_ports == [1])
        assert f.master.udp_port_types == [UDPPortPosition.TOP, UDPPortPosition.BOTTOM]
        rois = f.master.rois
        assert len(rois) == 1
        assert rois[0] == ROI(0, 512, 256, 512)

    with RawFile(test_data_path / "raw/eiger/2_modules_eiger_disabled_udp_port_master_0.json") as f:
        _, frame = f.read_rois() 

        assert(len(frame) == 2)
        assert frame[0].shape == (512, 512)
        assert frame[1].shape == (512, 512)
        assert (f.master.disabled_udp_ports == [1, 3, 5, 7])
        assert f.master.udp_port_types == [UDPPortPosition.LEFT, UDPPortPosition.RIGHT]
        rois = f.master.rois
        assert len(rois) == 2
        assert rois[0] == ROI(0, 512, 0, 512)
        assert rois[1] == ROI(1024, 1536, 0, 512)
