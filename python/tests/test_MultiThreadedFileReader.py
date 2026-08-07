# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import MultiThreadedFileReader


@pytest.fixture
def frame_file(tmp_path):
    data = np.arange(10 * 2 * 3, dtype=np.uint16).reshape(10, 2, 3)
    path = tmp_path / "frames.npy"
    np.save(path, data)
    return path, data


def test_reads_all_frames_in_order(frame_file):
    path, expected = frame_file
    reader = MultiThreadedFileReader(path, n_threads=2, chunk_size=3)

    first = reader.read()
    second = reader.read()
    exhausted = reader.read()

    assert np.array_equal(first, expected[:6])
    assert np.array_equal(second, expected[6:])
    assert exhausted.shape == (0, 2, 3)
    assert first.dtype == np.uint16
    assert reader.n_threads == 2
    assert reader.chunk_size == 3
    assert reader.total_frames == 10
    assert reader.source_total_frames == 10
    assert reader.rows == 2
    assert reader.cols == 3
    assert reader.bitdepth == 16
    assert reader.dtype == np.dtype(np.uint16)
    assert reader.bytes_per_frame == 12
    assert reader.total_bytes == expected.nbytes
    assert len(reader) == 10
    assert reader.tell() == 10
    assert reader.remaining_frames == 0
    assert reader.next_read_frames == 0
    assert reader.next_read_bytes == 0


def test_total_frame_limit(frame_file):
    path, expected = frame_file
    reader = MultiThreadedFileReader(
        path, n_threads=2, chunk_size=2, total_frames=7
    )

    assert np.array_equal(reader.read(), expected[:4])
    assert np.array_equal(reader.read_all(), expected[4:7])


def test_iteration_yields_one_chunk_per_thread(frame_file):
    path, expected = frame_file
    reader = MultiThreadedFileReader(path, n_threads=2, chunk_size=2)

    batches = list(reader)

    assert [len(batch) for batch in batches] == [4, 4, 2]
    assert np.array_equal(np.concatenate(batches), expected)


def test_seek_resets_iteration_position(frame_file):
    path, expected = frame_file
    reader = MultiThreadedFileReader(path, n_threads=2, chunk_size=2)

    reader.read()
    assert reader.tell() == 4

    reader.seek(1)
    assert reader.tell() == 1
    assert np.array_equal(reader.read(), expected[1:5])

    with pytest.raises(IndexError):
        reader.seek(11)


def test_context_manager_closes_worker_files(frame_file):
    path, expected = frame_file

    with MultiThreadedFileReader(path, n_threads=2, chunk_size=2) as reader:
        assert not reader.closed
        assert np.array_equal(reader.read(), expected[:4])

    assert reader.closed
    reader.close()
    with pytest.raises(RuntimeError):
        reader.read()
    with pytest.raises(RuntimeError):
        reader.seek(0)
    with pytest.raises(RuntimeError):
        with reader:
            pass


def test_explicit_zero_frame_limit(frame_file):
    path, expected = frame_file
    reader = MultiThreadedFileReader(
        path, n_threads=8, chunk_size=3, total_frames=0
    )

    actual = reader.read()
    assert actual.shape == (0, *expected.shape[1:])
    assert actual.dtype == expected.dtype


@pytest.mark.parametrize(
    "dtype", [np.int8, np.int32, np.uint64, np.float32, np.float64]
)
def test_preserves_numpy_dtype(tmp_path, dtype):
    expected = np.arange(4 * 2 * 3, dtype=dtype).reshape(4, 2, 3)
    path = tmp_path / "typed-frames.npy"
    np.save(path, expected)

    reader = MultiThreadedFileReader(path, n_threads=2, chunk_size=3)
    actual = reader.read()

    assert actual.dtype == expected.dtype
    assert reader.dtype == expected.dtype
    assert np.array_equal(actual, expected)


@pytest.mark.parametrize(
    ("n_threads", "chunk_size", "total_frames"),
    [(0, 1, None), (1, 0, None), (1, 1, 11)],
)
def test_invalid_configuration(
    frame_file, n_threads, chunk_size, total_frames
):
    path, _ = frame_file

    with pytest.raises(ValueError):
        MultiThreadedFileReader(
            path,
            n_threads=n_threads,
            chunk_size=chunk_size,
            total_frames=total_frames,
        )
