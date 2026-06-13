import numpy as np
import pytest

from aare import Pedestal_d, Pedestal_f


@pytest.mark.parametrize(
    ("pedestal_type", "expected_dtype"),
    [(Pedestal_d, np.float64), (Pedestal_f, np.float32)],
)
def test_numpy_array_minus_pedestal(pedestal_type, expected_dtype):
    pedestal = pedestal_type(2, 3)
    pedestal.push(np.array([[2, 4, 6], [8, 10, 12]], dtype=np.uint16))
    array = np.array([[12, 14, 16], [18, 20, 22]], dtype=np.uint16)

    result = array - pedestal

    np.testing.assert_array_equal(
        result, np.array([[10, 10, 10], [10, 10, 10]], dtype=expected_dtype)
    )
    assert result.dtype == expected_dtype


def test_numpy_array_minus_pedestal_rejects_incompatible_shape():
    pedestal = Pedestal_d(2, 3)
    array = np.zeros((2, 2), dtype=np.float64)

    with pytest.raises(ValueError):
        array - pedestal


def test_pedestal_exposes_mean_as_read_only_buffer():
    pedestal = Pedestal_d(2, 3)
    pedestal.push(np.array([[2, 4, 6], [8, 10, 12]], dtype=np.uint16))

    mean = np.asarray(pedestal)

    np.testing.assert_array_equal(mean, pedestal.view())
    assert np.shares_memory(mean, pedestal.view())
    assert not mean.flags.writeable
