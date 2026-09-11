import numpy as np
import pytest

from aare import Pedestal, Pedestal_d, Pedestal_f, Pedestal_i16


@pytest.mark.parametrize(
    ("dtype", "pedestal_type"),
    [(np.float64, Pedestal_d), (np.float32, Pedestal_f), (np.int16, Pedestal_i16)],
)
def test_pedestal_factory(dtype, pedestal_type):
    pedestal = Pedestal(rows=2, cols=3, n_samples=4, dtype=dtype)

    assert isinstance(pedestal, pedestal_type)
    assert pedestal.rows == 2
    assert pedestal.cols == 3
    assert pedestal.n_samples == 4


def test_pedestal_factory_defaults_to_double():
    pedestal = Pedestal(2, 3)

    assert isinstance(pedestal, Pedestal_d)
    assert pedestal.n_samples == 1000


def test_pedestal_factory_rejects_unbound_dtype():
    with pytest.raises(ValueError, match="Unsupported dtype for Pedestal"):
        Pedestal(2, 3, dtype=np.int32)


@pytest.mark.parametrize(
    ("pedestal_type", "expected_dtype"),
    [(Pedestal_d, np.float64), (Pedestal_f, np.float32), (Pedestal_i16, np.int16)],
)
def test_double_precision_moments(pedestal_type, expected_dtype):
    pedestal = pedestal_type(1, 1, 2)
    for value in [30000, 30003, 30002]:
        pedestal.push(np.array([[value]], dtype=np.uint16))

    assert pedestal.mean().dtype == expected_dtype
    assert pedestal.variance().dtype == expected_dtype
    assert pedestal.std().dtype == expected_dtype
    np.testing.assert_array_equal(
        pedestal.mean(), np.array([[30001.75]], dtype=expected_dtype)
    )
    np.testing.assert_array_equal(
        pedestal.variance(), np.array([[1.1875]], dtype=expected_dtype)
    )


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
