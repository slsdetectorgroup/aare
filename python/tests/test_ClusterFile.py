# SPDX-License-Identifier: MPL-2.0

import pytest 
import numpy as np
import boost_histogram as bh
import time
from pathlib import Path
import pickle
import struct
import gc
import weakref

from aare import ClusterFile, ROI
from conftest import test_data_path


@pytest.fixture
def iterator_file(tmp_path):
    fname = tmp_path / "iteration.clust"
    records = [
        (-11, []),
        (42, [(5, 6, 10), (7, 8, 20)]),
        (103, []),
        (104, [(5, 6, 30), (7, 8, 40)]),
        (105, []),
    ]
    fname.write_bytes(b"".join(
        struct.pack("@iI", number, len(clusters))
        + b"".join(struct.pack("@HH9i", x, y, *([value] * 9))
                   for x, y, value in clusters)
        for number, clusters in records
    ))
    return fname


def test_frames_preserve_empty_frames_and_frame_numbers(iterator_file):
    with ClusterFile(iterator_file) as reader:
        frames = reader.frames()
        assert iter(frames) is frames
        assert reader.tell() == 0
        results = list(frames)
        assert [frame.frame_number for frame in results] == [-11, 42, 103, 104, 105]
        assert [frame.size for frame in results] == [0, 2, 0, 2, 0]
        for _ in range(2):
            with pytest.raises(StopIteration):
                next(frames)
        assert list(reader.frames()) == []


@pytest.mark.parametrize("chunk_size, sizes", [(1, [1, 1, 1, 1]), (2, [2, 2]),
                                             (3, [3, 1]), (10, [4])])
@pytest.mark.parametrize("explicit", [False, True])
def test_chunks_cross_frames(iterator_file, chunk_size, sizes, explicit):
    with ClusterFile(iterator_file, chunk_size=chunk_size) as reader:
        chunks = reader.chunks(chunk_size=chunk_size) if explicit else reader.chunks()
        assert iter(chunks) is chunks
        assert reader.tell() == 0
        results = list(chunks)
        assert [chunk.size for chunk in results] == sizes
        data = np.concatenate([np.asarray(chunk) for chunk in results])
        np.testing.assert_array_equal(data["x"], [5, 7, 5, 7])
        np.testing.assert_array_equal(data["data"][:, 0, 0], [10, 20, 30, 40])
        for _ in range(2):
            with pytest.raises(StopIteration):
                next(chunks)


def test_chunk_override_preserves_default_iteration(iterator_file):
    with ClusterFile(iterator_file, chunk_size=2) as reader:
        assert iter(reader) is reader
        assert next(reader.chunks(1)).size == 1
        assert [chunk.size for chunk in reader] == [2, 1]


@pytest.mark.parametrize("use_roi", [False, True])
@pytest.mark.parametrize("use_noise", [False, True])
@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_iterators_apply_filters_and_gain(iterator_file, use_roi, use_noise, method):
    with ClusterFile(iterator_file, chunk_size=3) as reader:
        if use_roi:
            reader.set_roi(ROI(0, 6, 0, 12))
        if use_noise:
            reader.set_noise_map(np.full((12, 12), 15, dtype=np.int32))
        reader.set_gain_map(np.full((12, 12), 2.0))
        results = list(getattr(reader, method)())
        if method == "frames":
            assert [frame.frame_number for frame in results] == [-11, 42, 103, 104, 105]
        expected = [value / 2 for x, value in [(5, 10), (7, 20), (5, 30), (7, 40)]
                    if (not use_roi or x == 5) and (not use_noise or value > 15)]
        data = np.concatenate([np.asarray(result) for result in results])
        np.testing.assert_array_equal(data["data"][:, 0, 0], expected)


@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_iterators_handle_all_rejected_clusters(iterator_file, method):
    with ClusterFile(iterator_file) as reader:
        reader.set_roi(ROI(0, 1, 0, 1))
        results = list(getattr(reader, method)())
        assert len(results) == (5 if method == "frames" else 0)
        assert all(result.size == 0 for result in results)


@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_iterator_keeps_file_alive(iterator_file, method):
    reader = ClusterFile(iterator_file)
    owner = weakref.ref(reader)
    iterator = getattr(reader, method)()
    del reader
    gc.collect()
    assert owner() is not None
    results = list(iterator)
    assert sum(result.size for result in results) == 4
    del iterator
    gc.collect()
    assert owner() is None


@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_retained_results_and_numpy_views_own_storage(iterator_file, method):
    with ClusterFile(iterator_file, chunk_size=2) as reader:
        reader.read_frame()  # Skip the initial empty frame.
        iterator = getattr(reader, method)()
        first = next(iterator)
        array = np.asarray(first)
        expected = array.copy()
        results = list(iterator)
        np.testing.assert_array_equal(np.asarray(first), expected)
    del first, results, iterator, reader
    gc.collect()
    np.testing.assert_array_equal(array, expected)


def test_frame_iteration_resumes_without_reading_ahead(iterator_file):
    with ClusterFile(iterator_file) as reader:
        reader.read_frame()
        for frame in reader.frames():
            assert frame.frame_number == 42
            break
        assert reader.read_frame().frame_number == 103
        assert [frame.frame_number for frame in reader.frames()] == [104, 105]


def test_partial_chunk_must_be_completed_before_frame_iteration(iterator_file):
    with ClusterFile(iterator_file) as reader:
        for chunk in reader.chunks(1):
            assert chunk.size == 1
            break
        with pytest.raises(RuntimeError, match="clusters left"):
            next(reader.frames())
        assert reader.read_clusters(1).size == 1
        assert [frame.frame_number for frame in reader.frames()] == [103, 104, 105]


@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_empty_iterators(tmp_path, method):
    fname = tmp_path / "empty.clust"
    fname.touch()
    with ClusterFile(fname) as reader:
        iterator = getattr(reader, method)()
        for _ in range(2):
            with pytest.raises(StopIteration):
                next(iterator)


@pytest.mark.parametrize("method", ["frames", "chunks"])
@pytest.mark.parametrize("started", [False, True])
def test_iterators_reject_closed_files(iterator_file, method, started):
    reader = ClusterFile(iterator_file, chunk_size=1)
    iterator = getattr(reader, method)()
    if started:
        next(iterator)
    reader.close()
    with pytest.raises(RuntimeError, match="not opened for reading"):
        next(iterator)


@pytest.mark.parametrize("method", ["frames", "chunks"])
@pytest.mark.parametrize("mode", ["w", "a"])
def test_iterators_reject_writing_modes(tmp_path, method, mode):
    with ClusterFile(tmp_path / "output.clust", mode=mode) as reader:
        with pytest.raises(RuntimeError, match="not opened for reading"):
            next(getattr(reader, method)())


def test_chunk_sizes_must_be_positive(iterator_file):
    with ClusterFile(iterator_file, chunk_size=0) as reader:
        with pytest.raises(ValueError, match="greater than zero"):
            reader.chunks()
        with pytest.raises(ValueError, match="greater than zero"):
            reader.chunks(0)
        with pytest.raises(ValueError, match="greater than zero"):
            next(reader)
        with pytest.raises(TypeError):
            reader.chunks(-1)
        assert reader.tell() == 0
        assert reader.read_clusters(0).size == 0
        assert len(list(reader.frames())) == 5


@pytest.mark.parametrize("method", ["frames", "chunks"])
def test_iterators_defer_read_errors_until_next_result(tmp_path, method):
    fname = tmp_path / "truncated.clust"
    cluster = struct.pack("@HH9i", 5, 6, *range(9))
    frame = struct.pack("@iI", 42, 1) + cluster
    fname.write_bytes(frame + frame[:-1])
    with ClusterFile(fname, chunk_size=1) as reader:
        iterator = getattr(reader, method)()
        assert next(iterator).size == 1
        assert reader.tell() == len(frame)
        with pytest.raises(RuntimeError):
            next(iterator)


@pytest.mark.parametrize(
    "shape, dtype",
    [(shape, dtype)
     for shape in [(2, 2), (3, 3), (5, 5), (7, 7), (9, 9)]
     for dtype in [np.int32, np.float32, np.float64]]
    + [((3, 3), np.int16)],
)
def test_iterators_are_bound_for_each_cluster_type(tmp_path, shape, dtype):
    fname = tmp_path / "typed_frames.clust"
    values = list(range(shape[0] * shape[1]))
    record = struct.pack("@HH" + np.dtype(dtype).char * len(values), 5, 6, *values)
    fname.write_bytes((struct.pack("@iI", -42, 1) + record) * 3)
    with ClusterFile(fname, cluster_size=shape, dtype=dtype) as reader:
        frame = next(reader.frames())
        assert frame.frame_number == -42
        np.testing.assert_array_equal(np.asarray(frame)["data"].reshape(-1), values)
        chunks = list(reader.chunks())
        assert len(chunks) == 1
        assert chunks[0].size == 2
        np.testing.assert_array_equal(np.asarray(chunks[0])["data"].reshape(-1), values * 2)


def test_read_frame_returns_none_at_eof(tmp_path):
    fname = tmp_path / "empty.clust"
    fname.touch()

    with ClusterFile(fname) as f:
        assert f.read_frame() is None


def test_read_frame_raises_for_malformed_file(tmp_path):
    fname = tmp_path / "malformed.clust"
    fname.write_bytes(b"\x00")

    with ClusterFile(fname) as f, pytest.raises(RuntimeError):
        f.read_frame()


@pytest.mark.parametrize("use_roi", [False, True])
@pytest.mark.parametrize("method", ["read_clusters", "default", "frames", "chunks"])
@pytest.mark.parametrize(
    "size",
    [
        pytest.param(3, id="partial-frame-number"),
        pytest.param(4, id="missing-cluster-count"),
        pytest.param(7, id="partial-cluster-count"),
        pytest.param(48, id="missing-cluster-record"),
        pytest.param(87, id="partial-cluster-record"),
    ],
)
def test_chunk_reads_reject_incomplete_frames(tmp_path, use_roi, method, size):
    fname = tmp_path / "incomplete.clust"
    cluster = struct.pack("@HH9i", 5, 6, *range(9))
    frame = struct.pack("@iI", 42, 2) + cluster * 2
    fname.write_bytes(frame[:size])

    with ClusterFile(fname) as reader:
        if use_roi:
            reader.set_roi(ROI(0, 10, 0, 10))
        with pytest.raises(RuntimeError):
            if method == "default":
                next(reader)
            elif method == "read_clusters":
                reader.read_clusters(10)
            else:
                next(getattr(reader, method)())


@pytest.mark.withdata
def test_cluster_file(test_data_path): 
    """Test ClusterFile""" 
    f =  ClusterFile(test_data_path / "clust/single_frame_97_clustrers.clust") 
    assert f.estimate_n_clusters() == 97
    assert f.tell() == 0
    cv = f.read_clusters(10) #conversion does not work


    assert cv.frame_number == 135
    assert cv.size == 10

    #Known data 
    #frame_number, num_clusters   [135] 97
    #[  1 200] [0 1 2 3 4 5 6 7 8]
    #[  2 201] [ 9 10 11 12 13 14 15 16 17]
    #[  3 202] [18 19 20 21 22 23 24 25 26]
    #[  4 203] [27 28 29 30 31 32 33 34 35]
    #[  5 204] [36 37 38 39 40 41 42 43 44]
    #[  6 205] [45 46 47 48 49 50 51 52 53]
    #[  7 206] [54 55 56 57 58 59 60 61 62]
    #[  8 207] [63 64 65 66 67 68 69 70 71]
    #[  9 208] [72 73 74 75 76 77 78 79 80]
    #[ 10 209] [81 82 83 84 85 86 87 88 89]

    #conversion to numpy array
    arr = np.array(cv, copy = False)
    
    assert arr.size == 10
    for i in range(10):
        assert arr[i]['x'] == i+1

@pytest.mark.withdata
def test_read_clusters_and_fill_histogram(test_data_path): 
    # Create the histogram
    n_bins = 100
    xmin = -100
    xmax = 1e4
    hist_aare = bh.Histogram(bh.axis.Regular(n_bins, xmin, xmax))

    fname = test_data_path / "clust/beam_En700eV_-40deg_300V_10us_d0_f0_100.clust"

    #Read clusters and fill the histogram with pixel values
    with ClusterFile(fname, chunk_size = 10000) as f:
        for clusters in f:
            arr = np.array(clusters, copy = False)
            hist_aare.fill(arr['data'].flat)


    #Load the histogram from the pickle file
    with open(fname.with_suffix('.pkl'), 'rb') as f:
        hist_py = pickle.load(f)
        
    #Compare the two histograms
    assert hist_aare == hist_py
