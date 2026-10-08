# SPDX-License-Identifier: MPL-2.0
import pytest
import json
from aare import File, RawFile, RawSubFile, DetectorType, ROI, UDPPortPosition, xy
from aare import GapPixels, ModuleGaps, insert_gap_pixels
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


MODULE_ROWS = 2
MODULE_COLS = 3


def write_module_layout(directory, layout_rows, layout_cols, frames=2,
                        module_rows=MODULE_ROWS, module_cols=MODULE_COLS,
                        detector_type="Jungfrau"):
    """Write a raw file series with modules in the given layout.

    Modules are numbered as DetectorGeometry orders them, down each column
    first, and every module holds distinct values so misplacement is visible.
    Returns the master path and the expected image of frame 0.
    """
    master_path = directory / "run_master_0.json"
    master_path.write_text(json.dumps({
        "Version": 7.2,
        "Detector Type": detector_type,
        "Timing Mode": "auto",
        "Geometry": {"x": layout_cols, "y": layout_rows},
        "Image Size in bytes": module_rows * module_cols * 2,
        "Pixels": {"x": module_cols, "y": module_rows},
        "Max Frames Per File": 1,
        "Total Frames": frames,
        "Frames in File": frames,
        "Frame Padding": 1,
        "Frame Discard Policy": "nodiscard",
    }))
    expected = np.zeros((module_rows * layout_rows, module_cols * layout_cols), dtype=np.uint16)
    for module in range(layout_rows * layout_cols):
        module_row, module_col = module % layout_rows, module // layout_rows
        pixels = ((np.arange(module_rows * module_cols, dtype=np.uint32) + module * 10 + 1) % 65536).astype(np.uint16)
        expected[module_row * module_rows:(module_row + 1) * module_rows,
                 module_col * module_cols:(module_col + 1) * module_cols] = pixels.reshape(module_rows, module_cols)
        for index in range(frames):
            data_path = directory / f"run_d{module}_f{index}_0.raw"
            # 112 byte detector header followed by the module pixels
            data_path.write_bytes(bytes(112) + pixels.tobytes())
    return master_path, expected


@pytest.mark.parametrize("reader_type", [RawFile, File])
@pytest.mark.parametrize("legacy_master", [False, True])
@pytest.mark.parametrize("padding, policy, supported", [
    (0, "nodiscard", False),
    (0, "discard", False),
    (0, "discardpartial", True),
    (1, "nodiscard", True),
    (1, "discard", True),
    (1, "discardpartial", True),
])
def test_raw_frame_policy(small_raw_file, reader_type, legacy_master,
                          padding, policy, supported):
    master_path = small_raw_file
    if legacy_master:
        master_path = master_path.with_suffix(".raw")
        master_path.write_text(
            "Version : 6.4\n"
            "Detector Type : Jungfrau\n"
            "Timing Mode : auto\n"
            "Geometry : [1, 1]\n"
            "Image Size : 12\n"
            "Pixels : [3, 2]\n"
            "Dynamic Range : 16\n"
            "Max Frames Per File : 1\n"
            "Total Frames : 2\n"
            "Frames in File : 2\n"
            f"Frame Padding : {padding}\n"
            f"Frame Discard Policy : {policy}\n"
        )
    else:
        metadata = json.loads(master_path.read_text())
        metadata["Frame Padding"] = padding
        metadata["Frame Discard Policy"] = policy
        master_path.write_text(json.dumps(metadata))

    if supported:
        reader = reader_type(master_path)
        assert reader.total_frames == 2
        frame = reader.read_frame()
        if reader_type is RawFile:
            _, frame = frame
        np.testing.assert_array_equal(frame, np.arange(6).reshape(2, 3))
    else:
        message = "requires frame padding or discardpartial"
        with pytest.raises(RuntimeError, match=message) as error:
            reader_type(master_path)
        assert str(master_path) in str(error.value)

        for index in range(2):
            (master_path.parent / f"run_d0_f{index}_0.raw").unlink()
        with pytest.raises(RuntimeError, match=message) as error:
            reader_type(master_path)
        assert str(master_path) in str(error.value)


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


def test_raw_file_geometry(small_raw_file):
    metadata = json.loads(small_raw_file.read_text())
    metadata["Geometry"] = {"x": 1, "y": 2}
    small_raw_file.write_text(json.dumps(metadata))
    for index in range(2):
        data = (small_raw_file.parent / f"run_d0_f{index}_0.raw").read_bytes()
        (small_raw_file.parent / f"run_d1_f{index}_0.raw").write_bytes(data)

    reader = RawFile(small_raw_file)
    assert reader.geometry == xy(row=2, col=1)
    assert reader.master.detector_layout == xy(row=2, col=1)
    assert reader.master.udp_interfaces_per_module == xy(row=1, col=1)


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


@pytest.mark.parametrize("layout", [(1, 1), (2, 1), (1, 2), (2, 2), (3, 2)])
@pytest.mark.parametrize("reader_type", [RawFile, File])
def test_raw_file_assembles_module_layout(tmp_path, layout, reader_type):
    layout_rows, layout_cols = layout
    master_path, expected = write_module_layout(tmp_path, layout_rows, layout_cols)

    with reader_type(master_path) as reader:
        # RawFile binds rows/cols as methods (ROI overloads), File as properties
        rows = reader.rows() if reader_type is RawFile else reader.rows
        cols = reader.cols() if reader_type is RawFile else reader.cols
        assert (rows, cols) == expected.shape
        result = reader.read_frame()
        image = result[1] if reader_type is RawFile else result
        assert image.shape == expected.shape
        assert (image == expected).all()

        reader.seek(0)
        result = reader.read_n(2)
        images = result[1] if reader_type is RawFile else result
        assert images.shape == (2,) + expected.shape
        assert (images == expected).all()


@pytest.mark.parametrize("layout", [(1, 1), (2, 1), (1, 2)])
def test_raw_file_gap_pixels(tmp_path, layout):
    layout_rows, layout_cols = layout
    master_path, expected = write_module_layout(tmp_path, layout_rows, layout_cols,
                                                module_rows=512, module_cols=1024)
    gaps = GapPixels(fill_value=5)
    with RawFile(master_path, gap_pixels=gaps) as reader:
        assert reader.gap_pixels == gaps
        assert (reader.rows(), reader.cols()) == (512 * layout_rows + 2 * (layout_rows * 2 - 1),
                                                  1024 * layout_cols + 2 * (layout_cols * 4 - 1))
        header, image = reader.read_frame()
        assert header.size == layout_rows * layout_cols
        assert (image == insert_gap_pixels(expected, DetectorType.Jungfrau, gaps)).all()
        reader.seek(0)
        _, images = reader.read_n(2)
        assert images.shape == (2, reader.rows(), reader.cols())
        assert (images[1] == image).all()

    with RawFile(master_path) as reader:
        assert reader.gap_pixels is None
        assert (reader.rows(), reader.cols()) == expected.shape

    with RawFile(master_path, gap_pixels=True) as reader:
        assert reader.gap_pixels == GapPixels()
        _, image = reader.read_frame()
        assert (image == insert_gap_pixels(expected, DetectorType.Jungfrau)).all()

    with File(master_path, gap_pixels=True) as reader:
        assert (reader.rows, reader.cols) == insert_gap_pixels(expected, DetectorType.Jungfrau).shape
        assert (reader.read_frame() == insert_gap_pixels(expected, DetectorType.Jungfrau)).all()

    module_gaps = GapPixels(module_gaps=ModuleGaps(8, 36))
    with File(master_path, gap_pixels=module_gaps) as reader:
        assert (reader.read_frame() == insert_gap_pixels(expected, DetectorType.Jungfrau, module_gaps)).all()


def test_raw_file_gap_pixels_split_counts(tmp_path):
    master_path, expected = write_module_layout(tmp_path, 1, 1, module_rows=512, module_cols=1024)
    gaps = GapPixels(split_counts=True, seed=11)
    with RawFile(master_path, gap_pixels=gaps) as reader:
        _, image = reader.read_frame()
    assert (image == insert_gap_pixels(expected, DetectorType.Jungfrau, gaps)).all()
    assert image.sum() == expected.sum()


def test_raw_file_gap_pixels_unsupported_detector(tmp_path):
    master_path, _ = write_module_layout(tmp_path, 1, 1, detector_type="Moench")
    RawFile(master_path)
    with pytest.raises(ValueError):
        RawFile(master_path, gap_pixels=True)
    with pytest.raises(ValueError):
        File(master_path, gap_pixels=True)


@pytest.mark.withdata
def test_raw_file_gap_pixels_recorded_jungfrau(test_data_path):
    path = test_data_path / "raw/jungfrau/jungfrau_single_master_0.json"
    gaps = GapPixels(fill_value=3)
    with RawFile(path, gap_pixels=gaps) as gapped, RawFile(path) as plain:
        assert (gapped.rows(), gapped.cols()) == (514, 1030)
        _, image = gapped.read_frame()
        _, reference = plain.read_frame()
        assert image.shape == (514, 1030)
        assert (image == insert_gap_pixels(reference, DetectorType.Jungfrau, gaps)).all()


@pytest.mark.withdata
def test_raw_file_gap_pixels_recorded_rois(test_data_path):
    path = test_data_path / "raw/ROITestData/MultipleROIs/run_master_0.json"
    with RawFile(path, gap_pixels=True) as gapped, RawFile(path) as plain:
        assert gapped.num_rois == 2
        _, images = gapped.read_rois()
        _, references = plain.read_rois()
        rois = plain.master.rois
        assert images[0].shape == (303, 103)
        assert images[1].shape == (101, 103)
        for image, reference, roi in zip(images, references, rois):
            assert (image == insert_gap_pixels(reference, DetectorType.Jungfrau, roi=roi)).all()
        gapped.seek(0)
        _, frames = gapped.read_n_with_roi(2, roi_index=1)
        assert frames.shape == (2, 101, 103)
