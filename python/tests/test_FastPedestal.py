import numpy as np
import pytest

from aare import FastPedestal_d, FastPedestal_f


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
    pedestal.update_mean()

    expected_mean = np.array(
        [[3, 5, 7], [9, 11, 13]], dtype=expected_dtype
    )
    np.testing.assert_array_equal(pedestal.mean(), expected_mean)
    np.testing.assert_array_equal(pedestal.std(), np.ones((2, 3)))


def test_fast_pedestal_steady_state_push():
    pedestal = FastPedestal_d(1, 2, 2)
    pedestal.push_init(np.array([[2, 4]], dtype=np.uint16))
    pedestal.push_init(np.array([[4, 6]], dtype=np.uint16))
    pedestal.update_mean()

    pedestal.push(np.array([[6, 8]], dtype=np.uint16))

    np.testing.assert_array_equal(pedestal.mean(), [[4.5, 6.5]])


def test_fast_pedestal_exposes_read_only_buffer_and_subtraction():
    pedestal = FastPedestal_d(1, 2, 1)
    pedestal.push_init(np.array([[2, 4]], dtype=np.uint16))
    pedestal.update_mean()

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
