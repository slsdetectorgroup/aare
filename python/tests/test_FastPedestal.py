# SPDX-License-Identifier: MPL-2.0
import json

import numpy as np
import pytest

from aare import (
    FastPedestal,
    FastPedestal_d,
    FastPedestal_f,
    FastPedestal_i16,
    read_state_header,
)


@pytest.mark.parametrize(
    ("dtype", "pedestal_type"),
    [
        (np.float64, FastPedestal_d),
        (np.float32, FastPedestal_f),
        (np.int16, FastPedestal_i16),
    ],
)
def test_fast_pedestal_factory(dtype, pedestal_type):
    pedestal = FastPedestal(2, 3, n_samples=4, dtype=dtype)

    assert isinstance(pedestal, pedestal_type)
    assert pedestal.rows == 2
    assert pedestal.cols == 3
    assert pedestal.n_samples == 4


def test_fast_pedestal_factory_defaults_to_double():
    assert isinstance(FastPedestal(2, 3), FastPedestal_d)


def test_fast_pedestal_factory_rejects_unbound_dtype():
    with pytest.raises(ValueError, match="Unsupported dtype for FastPedestal"):
        FastPedestal(2, 3, dtype=np.int32)


@pytest.mark.parametrize(
    ("kwargs", "expected_n_samples"),
    [
        ({"rows": 2, "cols": 3}, 1000),
        ({"rows": 2, "cols": 3, "n_samples": 4}, 4),
    ],
)
def test_fast_pedestal_binding_accepts_constructor_keywords(
    kwargs, expected_n_samples
):
    pedestal = FastPedestal_d(**kwargs)

    assert pedestal.rows == 2
    assert pedestal.cols == 3
    assert pedestal.n_samples == expected_n_samples


@pytest.mark.parametrize(
    ("dtype", "pedestal_type", "expected_dtype"),
    [
        (np.float64, FastPedestal_d, np.float64),
        (np.float32, FastPedestal_f, np.float32),
        (np.int16, FastPedestal_i16, np.int16),
    ],
)
def test_fast_pedestal_factory_from_file(
    tmp_path, dtype, pedestal_type, expected_dtype
):
    frames = np.array(
        [[[100, 100]], [[2, 4]], [[4, 6]], [[5, 7]]], dtype=np.uint16
    )
    filename = tmp_path / "frames.npy"
    np.save(filename, frames)

    pedestal = FastPedestal.from_file(
        filename, n_samples=2, skip_first=1, dtype=dtype
    )

    assert isinstance(pedestal, pedestal_type)
    assert pedestal.ready
    assert pedestal.cur_samples == 2
    assert pedestal.mean().dtype == expected_dtype
    np.testing.assert_array_equal(pedestal.mean(), [[4, 6]])


def test_fast_pedestal_factory_from_file_rejects_unbound_dtype():
    with pytest.raises(ValueError, match="Unsupported dtype for FastPedestal"):
        FastPedestal.from_file("unused.npy", dtype=np.int32)


def test_fast_pedestal_from_file_rejects_skip_beyond_end(tmp_path):
    filename = tmp_path / "frames.npy"
    np.save(filename, np.zeros((1, 1, 1), dtype=np.uint16))

    with pytest.raises(RuntimeError, match="less frames"):
        FastPedestal.from_file(filename, n_samples=1, skip_first=2)


@pytest.mark.parametrize(
    ("pedestal_type", "expected_dtype"),
    [(FastPedestal_d, np.float64), (FastPedestal_f, np.float32)],
)
def test_fast_pedestal_initialization(pedestal_type, expected_dtype):
    pedestal = pedestal_type(2, 3, 2)
    first = np.array([[2, 4, 6], [8, 10, 12]], dtype=np.uint16)
    second = np.array([[4, 6, 8], [10, 12, 14]], dtype=np.uint16)

    pedestal.add_init_frame(first)
    pedestal.add_init_frame(second)


    expected_mean = np.array(
        [[3, 5, 7], [9, 11, 13]], dtype=expected_dtype
    )
    np.testing.assert_array_equal(pedestal.mean(), expected_mean)
    np.testing.assert_array_equal(pedestal.std(), np.ones((2, 3)))


def test_fast_pedestal_steady_state_push_ema():
    pedestal = FastPedestal_d(1, 2, 2)
    pedestal.add_init_frame(np.array([[2, 4]], dtype=np.uint16))
    pedestal.add_init_frame(np.array([[4, 6]], dtype=np.uint16))


    pedestal.push_ema(np.array([[6, 8]], dtype=np.uint16))

    np.testing.assert_array_equal(pedestal.mean(), [[4.5, 6.5]])


def test_fast_pedestal_exposes_read_only_buffer_and_subtraction():
    pedestal = FastPedestal_d(1, 2, 1)
    pedestal.add_init_frame(np.array([[2, 4]], dtype=np.uint16))


    view = np.asarray(pedestal)
    result = np.array([[12, 14]], dtype=np.uint16) - pedestal

    np.testing.assert_array_equal(view, [[2, 4]])
    np.testing.assert_array_equal(result, [[10, 10]])
    assert np.shares_memory(view, pedestal.view())
    assert not view.flags.writeable


def test_fast_pedestal_rejects_wrong_shape():
    pedestal = FastPedestal_d(2, 3)

    with pytest.raises(RuntimeError, match="shape"):
        pedestal.add_init_frame(np.zeros((2, 2), dtype=np.uint16))


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
def test_fast_pedestal_std_uses_double_variance(dtype):
    pedestal = FastPedestal(1, 1, n_samples=2, dtype=dtype)
    pedestal.add_init_frame(np.array([[0]], dtype=np.uint16))
    pedestal.add_init_frame(np.array([[1000]], dtype=np.uint16))

    assert pedestal.std().dtype == dtype
    np.testing.assert_array_equal(pedestal.std(), [[500]])

    pedestal.push_ema(np.array([[1000]], dtype=np.uint16))
    expected = np.array([[np.sqrt(187500.0)]], dtype=dtype)
    np.testing.assert_array_equal(pedestal.std(), expected)


def _push(pedestal, frame):
    if pedestal.ready:
        pedestal.push_ema(frame)
    else:
        pedestal.add_init_frame(frame)


def _test_frames(n_frames, shape=(3, 4)):
    rng = np.random.default_rng(42)
    return rng.integers(1000, 1100, size=(n_frames, *shape), dtype=np.uint16)


def _read_state_file(fname):
    """Read a state file with NumPy only, as shown in the documentation."""
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


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("frames_before", [0, 3, 5, 8])
def test_fast_pedestal_continues_from_state_file(tmp_path, dtype, frames_before):
    # Empty, initializing, just ready and updated after becoming ready
    frames = _test_frames(frames_before + 10)
    original = FastPedestal(3, 4, n_samples=5, dtype=dtype)
    for frame in frames[:frames_before]:
        _push(original, frame)

    original.save_state(tmp_path / "pedestal.aare")
    restored = FastPedestal.load_state(tmp_path / "pedestal.aare", dtype=dtype)

    assert type(restored) is type(original)
    assert (restored.rows, restored.cols) == (3, 4)
    assert restored.n_samples == 5
    assert restored.cur_samples == original.cur_samples
    assert restored.ready == original.ready

    for frame in frames[frames_before:]:
        _push(original, frame)
        _push(restored, frame)

    np.testing.assert_array_equal(restored.mean(), original.mean())
    np.testing.assert_array_equal(restored.std(), original.std())


def test_fast_pedestal_state_file_does_not_depend_on_dtype(tmp_path):
    original = FastPedestal(3, 4, n_samples=5, dtype=np.float64)
    for frame in _test_frames(8):
        _push(original, frame)

    # A str is accepted as well as a Path
    original.save_state(str(tmp_path / "pedestal.aare"))
    restored = FastPedestal.load_state(
        str(tmp_path / "pedestal.aare"), dtype=np.float32
    )

    assert isinstance(restored, FastPedestal_f)
    np.testing.assert_array_equal(
        restored.mean(), original.mean().astype(np.float32)
    )


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
def test_fast_pedestal_load_state_defaults_to_the_stored_dtype(tmp_path, dtype):
    original = FastPedestal(2, 3, dtype=dtype)
    original.save_state(tmp_path / "pedestal.aare")

    restored = FastPedestal.load_state(tmp_path / "pedestal.aare")

    assert type(restored) is type(original)


def test_fast_pedestal_load_state_in_place_keeps_the_dtype(tmp_path):
    FastPedestal(2, 3, dtype=np.float32).save_state(tmp_path / "pedestal.aare")

    pedestal = FastPedestal(2, 3, dtype=np.int16)
    pedestal.load_state(tmp_path / "pedestal.aare")

    assert isinstance(pedestal, FastPedestal_i16)


def test_read_state_header(tmp_path):
    pedestal = FastPedestal(2, 3, n_samples=7, dtype=np.int16)
    pedestal.add_init_frame(_test_frames(1, shape=(2, 3))[0])
    pedestal.save_state(tmp_path / "pedestal.aare")

    header = read_state_header(tmp_path / "pedestal.aare")

    assert header["format_version"] == 1
    assert header["type"] == "FastPedestal"
    assert isinstance(header["aare_version"], str)
    assert header["state"]["dtype"] == "<i2"
    assert header["state"]["n_samples"] == 7
    assert header["state"]["cur_samples"] == 1
    assert header["state"]["sum"] == {"dtype": "<f8", "shape": [2, 3], "offset": 0}
    assert header["state"]["sum2"]["offset"] == 64

    with pytest.raises(RuntimeError, match="could not open"):
        read_state_header(tmp_path / "missing.aare")
    np.save(tmp_path / "frames.npy", _test_frames(2))
    with pytest.raises(RuntimeError, match="not an aare state file"):
        read_state_header(tmp_path / "frames.npy")


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("frames_before", [0, 3, 5, 8])
def test_fast_pedestal_loads_state_file_in_place(tmp_path, dtype, frames_before):
    frames = _test_frames(frames_before + 10)
    original = FastPedestal(3, 4, n_samples=5, dtype=np.float64)
    for frame in frames[:frames_before]:
        _push(original, frame)
    original.save_state(tmp_path / "pedestal.aare")

    # A pedestal that already holds other data, with another n_samples
    pedestal = FastPedestal(3, 4, n_samples=2, dtype=dtype)
    for frame in _test_frames(4) + 500:
        _push(pedestal, frame)
    assert pedestal.ready

    assert pedestal.load_state(tmp_path / "pedestal.aare") is None

    assert pedestal.n_samples == 5
    assert pedestal.cur_samples == original.cur_samples
    assert pedestal.ready == original.ready

    reference = FastPedestal.load_state(tmp_path / "pedestal.aare", dtype=dtype)
    for frame in frames[frames_before:]:
        _push(pedestal, frame)
        _push(reference, frame)
    np.testing.assert_array_equal(pedestal.mean(), reference.mean())
    np.testing.assert_array_equal(pedestal.std(), reference.std())


def test_fast_pedestal_load_state_in_place_keeps_pedestal_on_error(tmp_path):
    frames = _test_frames(3)
    pedestal = FastPedestal(3, 4, n_samples=3)
    for frame in frames:
        pedestal.add_init_frame(frame)
    mean = pedestal.mean()

    FastPedestal(4, 3, n_samples=7).save_state(tmp_path / "other_shape.aare")
    with pytest.raises(RuntimeError, match=r"expected \(3, 4\)"):
        pedestal.load_state(tmp_path / "other_shape.aare")
    with pytest.raises(RuntimeError, match="could not open"):
        pedestal.load_state(tmp_path / "missing.aare")

    assert pedestal.n_samples == 3
    assert pedestal.ready
    np.testing.assert_array_equal(pedestal.mean(), mean)


def test_fast_pedestal_state_file_is_readable_with_numpy(tmp_path):
    frames = _test_frames(5)
    pedestal = FastPedestal(3, 4, n_samples=5)
    for frame in frames[:3]:
        pedestal.add_init_frame(frame)
    pedestal.save_state(tmp_path / "pedestal.aare")

    content = _read_state_file(tmp_path / "pedestal.aare")

    assert content["format_version"] == 1
    assert content["type"] == "FastPedestal"
    assert isinstance(content["aare_version"], str)
    state = content["state"]
    assert state["dtype"] == "<f8"
    assert state["n_samples"] == 5
    assert state["cur_samples"] == 3
    values = frames[:3].astype(np.float64)
    assert state["sum"].dtype == np.float64
    np.testing.assert_array_equal(state["sum"], values.sum(axis=0))
    np.testing.assert_array_equal(state["sum2"], (values**2).sum(axis=0))

    # Once ready, the mean is sum / n_samples. The pedestal multiplies by
    # the reciprocal, which can differ from a division in the last bit.
    for frame in frames[3:]:
        pedestal.add_init_frame(frame)
    pedestal.save_state(tmp_path / "pedestal.aare")
    state = _read_state_file(tmp_path / "pedestal.aare")["state"]
    np.testing.assert_array_equal(
        state["sum"] * (1.0 / state["n_samples"]), pedestal.mean()
    )


def test_fast_pedestal_load_state_rejects_other_files(tmp_path):
    with pytest.raises(RuntimeError, match="could not open"):
        FastPedestal.load_state(tmp_path / "missing.aare")

    np.save(tmp_path / "frames.npy", _test_frames(2))
    with pytest.raises(RuntimeError, match="not an aare state file"):
        FastPedestal.load_state(tmp_path / "frames.npy")


def test_fast_pedestal_load_state_rejects_truncated_file(tmp_path):
    fname = tmp_path / "pedestal.aare"
    FastPedestal(3, 4).save_state(fname)
    fname.write_bytes(fname.read_bytes()[:-1])

    with pytest.raises(RuntimeError, match="truncated or damaged"):
        FastPedestal.load_state(fname)


def test_fast_pedestal_load_state_rejects_unbound_dtype(tmp_path):
    FastPedestal(2, 3).save_state(tmp_path / "pedestal.aare")

    with pytest.raises(ValueError, match="Unsupported dtype for FastPedestal"):
        FastPedestal.load_state(tmp_path / "pedestal.aare", dtype=np.int32)
