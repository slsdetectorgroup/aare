import numpy as np
import pytest

from aare import (
    FastPedestal,
    FastPedestal_d,
    FastPedestal_f,
    FastPedestal_i16,
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

    pedestal.push_init(first)
    pedestal.push_init(second)


    expected_mean = np.array(
        [[3, 5, 7], [9, 11, 13]], dtype=expected_dtype
    )
    np.testing.assert_array_equal(pedestal.mean(), expected_mean)
    np.testing.assert_array_equal(pedestal.std(), np.ones((2, 3)))


def test_fast_pedestal_steady_state_push():
    pedestal = FastPedestal_d(1, 2, 2)
    pedestal.push_init(np.array([[2, 4]], dtype=np.uint16))
    pedestal.push_init(np.array([[4, 6]], dtype=np.uint16))


    pedestal.push(np.array([[6, 8]], dtype=np.uint16))

    np.testing.assert_array_equal(pedestal.mean(), [[4.5, 6.5]])


def test_fast_pedestal_exposes_read_only_buffer_and_subtraction():
    pedestal = FastPedestal_d(1, 2, 1)
    pedestal.push_init(np.array([[2, 4]], dtype=np.uint16))


    view = np.asarray(pedestal)
    result = np.array([[12, 14]], dtype=np.uint16) - pedestal

    np.testing.assert_array_equal(view, [[2, 4]])
    np.testing.assert_array_equal(result, [[10, 10]])
    assert np.shares_memory(view, pedestal.view())
    assert not view.flags.writeable


def test_fast_pedestal_rejects_wrong_shape():
    pedestal = FastPedestal_d(2, 3)

    with pytest.raises(RuntimeError, match="shape"):
        pedestal.push_init(np.zeros((2, 2), dtype=np.uint16))
