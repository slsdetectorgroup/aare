# SPDX-License-Identifier: MPL-2.0
import json

import numpy as np
import pytest

from aare import ClusterFinder, FastPedestal, _aare, read_state_header

SHAPE = (10, 12)
N_SAMPLES = 5


def _test_frames(n_frames, shape=SHAPE, seed=0):
    """Frames with a pedestal near 1000, a few ADU of noise and, after the
    first N_SAMPLES frames, one photon each."""
    rng = np.random.default_rng(seed)
    frames = rng.integers(1000, 1008, size=(n_frames, *shape)).astype(np.uint16)
    for i in range(N_SAMPLES, n_frames):
        frames[i, 2 + i % 5, 2 + (3 * i) % 6] += 300
    return frames


def _process(finder, frame, frame_number=0):
    """Use a frame for the pedestal until that is ready, then find clusters."""
    if finder.pedestal_ready:
        finder.find_clusters(frame, frame_number)
    else:
        finder.push_pedestal_frame(frame)


def _finder(dtype=np.int32, cluster_size=(3, 3), n_sigma=4.1, shape=SHAPE):
    return ClusterFinder(
        shape,
        cluster_size=cluster_size,
        n_sigma=n_sigma,
        dtype=dtype,
        min_pedestal_samples=N_SAMPLES,
    )


def _clusters(finder):
    return np.array(finder.steal_clusters())


def _read_state_file(fname):
    """The reader documented in docs/src/StateFile.rst, using NumPy only."""
    with open(fname, "rb") as f:
        if f.read(8) != b"AARESTAT":
            raise ValueError("not an aare state file")
        header_length = int.from_bytes(f.read(8), "little")
        header = json.loads(f.read(header_length))
    data_start = 16 + header_length

    def resolve(node):
        if isinstance(node, dict):
            if node.keys() == {"dtype", "shape", "offset"}:
                return np.fromfile(
                    fname,
                    dtype=node["dtype"],
                    count=int(np.prod(node["shape"])),
                    offset=data_start + node["offset"],
                ).reshape(node["shape"])
            return {key: resolve(value) for key, value in node.items()}
        if isinstance(node, list):
            return [resolve(value) for value in node]
        return node

    return resolve(header)


@pytest.mark.parametrize("dtype", [np.int32, np.float32, np.float64])
@pytest.mark.parametrize("frames_before", [0, 3, 5, 12])
def test_cluster_finder_continues_from_state_file(tmp_path, dtype, frames_before):
    # Empty, initializing, just ready and after finding clusters
    frames = _test_frames(frames_before + 10)
    original = _finder(dtype)
    for i, frame in enumerate(frames[:frames_before]):
        _process(original, frame, i)
    original.steal_clusters()

    original.save_state(tmp_path / "finder.aare")
    restored = ClusterFinder.load_state(tmp_path / "finder.aare")

    assert type(restored) is type(original)
    assert restored.nSigma == original.nSigma
    assert restored.pedestal_ready == original.pedestal_ready

    for i, frame in enumerate(frames[frames_before:], start=frames_before):
        _process(original, frame, i)
        _process(restored, frame, i)

    np.testing.assert_array_equal(restored.pedestal, original.pedestal)
    np.testing.assert_array_equal(restored.noise, original.noise)
    expected = _clusters(original)
    assert expected.size > 0
    np.testing.assert_array_equal(_clusters(restored), expected)


def test_cluster_finder_state_file_keeps_the_threshold(tmp_path):
    # A photon of 300 ADU is found with the threshold computed from the
    # pedestal frames. Recomputed with nSigma changed behind its back it
    # would not be, so the restored finder must use the stored threshold.
    frames = _test_frames(N_SAMPLES + 1)
    original = _finder(n_sigma=4.1)
    for frame in frames[:N_SAMPLES]:
        original.push_pedestal_frame(frame)
    original.save_state(tmp_path / "finder.aare")

    content = _read_state_file(tmp_path / "finder.aare")
    np.testing.assert_array_equal(
        content["state"]["threshold"], original.noise * 4.1
    )

    restored = ClusterFinder.load_state(tmp_path / "finder.aare")
    original.find_clusters(frames[N_SAMPLES])
    restored.find_clusters(frames[N_SAMPLES])
    expected = _clusters(original)
    assert expected.size > 0
    np.testing.assert_array_equal(_clusters(restored), expected)


@pytest.mark.parametrize("cluster_size", [(3, 3), (5, 5), (9, 9)])
@pytest.mark.parametrize("dtype", [np.int32, np.float32, np.float64])
def test_cluster_finder_load_state_defaults_to_the_stored_type(
    tmp_path, cluster_size, dtype
):
    original = _finder(dtype, cluster_size)
    original.save_state(tmp_path / "finder.aare")

    restored = ClusterFinder.load_state(tmp_path / "finder.aare")

    assert type(restored) is type(original)


def test_cluster_finder_load_state_takes_another_dtype(tmp_path):
    frames = _test_frames(8)
    original = _finder(np.int32)
    for frame in frames:
        _process(original, frame)

    # A str is accepted as well as a Path
    original.save_state(str(tmp_path / "finder.aare"))
    restored = ClusterFinder.load_state(
        str(tmp_path / "finder.aare"), dtype=np.float64, capacity=10
    )

    assert isinstance(restored, _aare.ClusterFinder_Cluster3x3d)
    np.testing.assert_array_equal(restored.pedestal, original.pedestal)
    np.testing.assert_array_equal(restored.noise, original.noise)


def test_cluster_finder_load_state_rejects_unbound_types(tmp_path):
    _finder(np.int32).save_state(tmp_path / "finder.aare")
    with pytest.raises(ValueError, match="Unsupported"):
        ClusterFinder.load_state(tmp_path / "finder.aare", dtype=np.uint8)

    # int16 clusters exist for 3x3 only
    _finder(np.float32, (5, 5)).save_state(tmp_path / "finder5.aare")
    with pytest.raises(ValueError, match="Unsupported"):
        ClusterFinder.load_state(tmp_path / "finder5.aare", dtype=np.int16)


def test_cluster_finder_state_header(tmp_path):
    finder = _finder(np.float32, (5, 5), n_sigma=3.5)
    finder.push_pedestal_frame(_test_frames(1)[0])
    finder.save_state(tmp_path / "finder.aare")

    header = read_state_header(tmp_path / "finder.aare")

    assert header["format_version"] == 1
    assert header["type"] == "ClusterFinder"
    state = header["state"]
    assert state["cluster_dtype"] == "<f4"
    assert state["cluster_size_x"] == 5
    assert state["cluster_size_y"] == 5
    assert state["n_sigma"] == 3.5
    assert state["threshold"] == {
        "dtype": "<f8",
        "shape": list(SHAPE),
        "offset": 0,
    }
    assert state["pedestal"]["dtype"] == "<f8"
    assert state["pedestal"]["n_samples"] == N_SAMPLES
    assert state["pedestal"]["cur_samples"] == 1
    assert state["pedestal"]["sum"]["shape"] == list(SHAPE)


def test_cluster_finder_state_file_is_readable_with_numpy(tmp_path):
    frames = _test_frames(N_SAMPLES)
    finder = _finder()
    for frame in frames:
        finder.push_pedestal_frame(frame)
    finder.save_state(tmp_path / "finder.aare")

    state = _read_state_file(tmp_path / "finder.aare")["state"]

    pedestal = state["pedestal"]
    assert pedestal["cur_samples"] == pedestal["n_samples"] == N_SAMPLES
    np.testing.assert_array_equal(pedestal["sum"], frames.sum(axis=0))
    np.testing.assert_allclose(
        pedestal["sum"] / pedestal["n_samples"], finder.pedestal, rtol=1e-14
    )
    np.testing.assert_array_equal(state["threshold"], finder.noise * 4.1)


@pytest.mark.parametrize("dtype", [np.int32, np.float64])
def test_cluster_finder_loads_state_file_in_place(tmp_path, dtype):
    frames = _test_frames(22)
    original = _finder(np.int32, n_sigma=4.1)
    for i, frame in enumerate(frames[:12]):
        _process(original, frame, i)
    original.steal_clusters()
    original.save_state(tmp_path / "finder.aare")

    # A finder that already holds other data, settings and clusters
    finder = ClusterFinder(SHAPE, n_sigma=3, dtype=dtype, min_pedestal_samples=2)
    for frame in _test_frames(6, seed=1) + 500:
        _process(finder, frame)
    assert finder.pedestal_ready

    assert finder.load_state(tmp_path / "finder.aare") is None

    assert type(finder) is type(_finder(dtype))
    assert finder.nSigma == 4.1
    # The clusters it had found are dropped
    assert finder.steal_clusters().size == 0

    reference = ClusterFinder.load_state(tmp_path / "finder.aare", dtype=dtype)
    for i, frame in enumerate(frames[12:], start=12):
        _process(finder, frame, i)
        _process(reference, frame, i)
    np.testing.assert_array_equal(finder.pedestal, reference.pedestal)
    expected = _clusters(reference)
    assert expected.size > 0
    np.testing.assert_array_equal(_clusters(finder), expected)


def test_cluster_finder_load_state_in_place_keeps_finder_on_error(tmp_path):
    frames = _test_frames(8)
    finder = _finder()
    reference = _finder()
    for frame in frames:
        _process(finder, frame)
        _process(reference, frame)
    pedestal = finder.pedestal

    _finder(cluster_size=(5, 5)).save_state(tmp_path / "other_size.aare")
    with pytest.raises(RuntimeError, match="5x5 clusters.*3x3 clusters"):
        finder.load_state(tmp_path / "other_size.aare")
    _finder(shape=(12, 10)).save_state(tmp_path / "other_shape.aare")
    with pytest.raises(RuntimeError, match=r"expected \(10, 12\)"):
        finder.load_state(tmp_path / "other_shape.aare")
    with pytest.raises(RuntimeError, match="could not open"):
        finder.load_state(tmp_path / "missing.aare")

    assert finder.nSigma == 4.1
    np.testing.assert_array_equal(finder.pedestal, pedestal)
    expected = _clusters(reference)
    assert expected.size > 0
    np.testing.assert_array_equal(_clusters(finder), expected)


def test_cluster_finder_from_state_file_needs_the_cluster_size(tmp_path):
    _finder(cluster_size=(5, 5)).save_state(tmp_path / "finder.aare")

    with pytest.raises(RuntimeError, match="5x5 clusters.*3x3 clusters"):
        _aare.ClusterFinder_Cluster3x3i.from_state_file(tmp_path / "finder.aare")


def test_cluster_finder_load_state_rejects_other_files(tmp_path):
    FastPedestal(3, 4).save_state(tmp_path / "pedestal.aare")
    with pytest.raises(RuntimeError, match="holds the state of a FastPedestal"):
        ClusterFinder.load_state(tmp_path / "pedestal.aare")

    _finder().save_state(tmp_path / "finder.aare")
    with pytest.raises(RuntimeError, match="holds the state of a ClusterFinder"):
        FastPedestal.load_state(tmp_path / "finder.aare")

    np.save(tmp_path / "frames.npy", _test_frames(2))
    with pytest.raises(RuntimeError, match="not an aare state file"):
        ClusterFinder.load_state(tmp_path / "frames.npy")
    with pytest.raises(RuntimeError, match="could not open"):
        ClusterFinder.load_state(tmp_path / "missing.aare")


def test_cluster_finder_load_state_rejects_truncated_file(tmp_path):
    finder = _finder()
    finder.save_state(tmp_path / "finder.aare")
    content = (tmp_path / "finder.aare").read_bytes()
    (tmp_path / "truncated.aare").write_bytes(content[:-8])

    with pytest.raises(RuntimeError, match="truncated or damaged"):
        ClusterFinder.load_state(tmp_path / "truncated.aare")
