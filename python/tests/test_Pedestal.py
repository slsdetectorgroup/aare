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


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("name", ["rows", "cols", "n_samples"])
def test_pedestal_factory_rejects_negative_parameters(dtype, name):
    parameters = dict(rows=1, cols=1, n_samples=10)
    parameters[name] = -1
    with pytest.raises(TypeError):
        Pedestal(**parameters, dtype=dtype)


@pytest.mark.parametrize("pedestal_type", [Pedestal_d, Pedestal_f, Pedestal_i16])
@pytest.mark.parametrize(
    "args", [(-1, 1), (1, -1), (-1, 1, 10), (1, -1, 10), (1, 1, -1)]
)
def test_pedestal_constructor_rejects_negative_parameters(pedestal_type, args):
    with pytest.raises(TypeError):
        pedestal_type(*args)


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
def test_pedestal_std_stays_finite_after_settling(dtype):
    pedestal = Pedestal(1, 1, n_samples=10, dtype=dtype)
    pedestal.push(np.array([[16382]], dtype=np.uint16))
    frame = np.array([[16383]], dtype=np.uint16)
    for _ in range(201):
        pedestal.push(frame)

    noise = pedestal.std().item()
    assert np.isfinite(noise)
    assert 0 <= noise < 1e-3


@pytest.mark.parametrize(
    ("pedestal_type", "expected_dtype"),
    [(Pedestal_d, np.float64), (Pedestal_f, np.float32), (Pedestal_i16, np.int16)],
)
def test_double_precision_moments(pedestal_type, expected_dtype):
    pedestal = pedestal_type(1, 1, 2)
    for value in [30000, 30003, 30002]:
        pedestal.push(np.array([[value]], dtype=np.uint16))

    assert pedestal.mean().dtype == expected_dtype
    assert pedestal.std().dtype == expected_dtype
    np.testing.assert_array_equal(
        pedestal.mean(), np.array([[30001.75]], dtype=expected_dtype)
    )
    np.testing.assert_allclose(
        pedestal.std(),
        np.array([[np.sqrt(1.1875)]], dtype=expected_dtype),
        rtol=1e-6,
    )


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("method", ["push", "push_with_threshold"])
@pytest.mark.parametrize("shape", [(2, 2), (3, 2)])
def test_pedestal_rejects_mismatched_frame_shapes(dtype, method, shape):
    pedestal = Pedestal(2, 3, dtype=dtype)
    initial = np.full((2, 3), 7, dtype=np.uint16)
    pedestal.push(initial)
    frame = np.zeros(shape, dtype=np.uint16)
    args = (
        (np.full((2, 3), 10, dtype=dtype),)
        if method == "push_with_threshold"
        else ()
    )

    with pytest.raises(
        RuntimeError, match="Frame shape does not match pedestal shape"
    ):
        getattr(pedestal, method)(frame, *args)

    np.testing.assert_array_equal(pedestal.mean(), initial)


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


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("method", ["push", "push_with_threshold"])
def test_pedestal_push_accepts_contiguous_frames(dtype, method):
    pedestal = Pedestal(2, 3, dtype=dtype)
    frame = np.arange(6, dtype=np.uint16).reshape(2, 3)
    args = (
        (np.full((2, 3), 10, dtype=dtype),)
        if method == "push_with_threshold"
        else ()
    )

    getattr(pedestal, method)(frame, *args)

    np.testing.assert_array_equal(pedestal.mean(), frame)


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("input_name", ["push_frame", "threshold_frame", "threshold"])
@pytest.mark.parametrize("layout", ["transpose", "slice", "reverse"])
def test_pedestal_rejects_noncontiguous_inputs(dtype, input_name, layout):
    pedestal = Pedestal(2, 3, dtype=dtype)
    frame = np.zeros((2, 3), dtype=np.uint16)
    threshold = np.full((2, 3), 10, dtype=dtype)
    input_dtype = dtype if input_name == "threshold" else np.uint16
    if layout == "transpose":
        invalid = np.ones((3, 2), dtype=input_dtype).T
    elif layout == "slice":
        invalid = np.ones((2, 6), dtype=input_dtype)[:, ::2]
    else:
        invalid = np.ones((4, 6), dtype=input_dtype)[:2, :3][:, ::-1]
    assert not invalid.flags.c_contiguous

    with pytest.raises(TypeError):
        if input_name == "push_frame":
            pedestal.push(invalid)
        elif input_name == "threshold_frame":
            pedestal.push_with_threshold(invalid, threshold)
        else:
            pedestal.push_with_threshold(frame, invalid)

    np.testing.assert_array_equal(pedestal.mean(), np.zeros((2, 3)))


@pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int16])
@pytest.mark.parametrize("input_name", ["push_frame", "threshold_frame", "threshold"])
@pytest.mark.parametrize("shape", [(), (6,), (2, 3, 2)])
def test_pedestal_rejects_inputs_with_wrong_ndim(dtype, input_name, shape):
    pedestal = Pedestal(2, 3, dtype=dtype)
    frame = np.ones((2, 3), dtype=np.uint16)
    threshold = np.full((2, 3), 10, dtype=dtype)
    input_dtype = dtype if input_name == "threshold" else np.uint16
    invalid = np.ones(shape, dtype=input_dtype)
    name = "Threshold" if input_name == "threshold" else "Frame"

    with pytest.raises(ValueError, match=f"{name} must be 2-dimensional"):
        if input_name == "push_frame":
            pedestal.push(invalid)
        elif input_name == "threshold_frame":
            pedestal.push_with_threshold(invalid, threshold)
        else:
            pedestal.push_with_threshold(frame, invalid)

    np.testing.assert_array_equal(pedestal.mean(), np.zeros((2, 3)))
