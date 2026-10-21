# SPDX-License-Identifier: MPL-2.0
import gc
import weakref

import numpy as np
import pytest

from aare import PixelHistogram_u32, PixelHistogramCUDA, _cuda_available

pytestmark = pytest.mark.skipif(
    not _cuda_available(), reason="aare built without AARE_CUDA")

# Non-square, and integer-valued frames on 10-wide bins: every value is
# exact in float32 and float64, so the GPU's float axis and the CPU's double
# axis put each value in the same bin.
ROWS, COLS, N_FRAMES = 7, 13, 50
N_BINS, XMIN, XMAX = 120, 3900.0, 5100.0


@pytest.fixture
def frames():
    rng = np.random.default_rng(12345)
    base = rng.uniform(4000, 5000, (ROWS, COLS))
    noise = rng.normal(0, 10, (N_FRAMES, ROWS, COLS))
    return np.round(base + noise).astype(np.uint16)


def make_gpu(dtype, storage_dtype=np.uint32, batch_size=8):
    return PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN,
                              xmax=XMAX, dtype=dtype,
                              storage_dtype=storage_dtype,
                              batch_size=batch_size)


def cpu_values(frames):
    cpu = PixelHistogram_u32(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN,
                             xmax=XMAX)
    for frame in frames:
        cpu.fill_async(frame.astype(np.float64))
    return cpu.values()


@pytest.mark.parametrize("dtype", [np.uint16, np.float32, np.float64])
def test_matches_cpu(frames, dtype):
    data = frames.astype(dtype)
    gpu = make_gpu(dtype)
    gpu.fill_histogram(data)
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))
    assert gpu.dtype == np.dtype(dtype)


@pytest.mark.parametrize("dtype, axis_dtype", [(np.uint16, np.float32),
                                               (np.float32, np.float32),
                                               (np.float64, np.float64)])
def test_axis_is_floating_point(dtype, axis_dtype):
    gpu = make_gpu(dtype)
    edges = gpu.bin_edges()
    assert edges.dtype == axis_dtype
    assert edges.shape == (N_BINS + 1,)
    assert edges[0] == XMIN and edges[-1] == XMAX
    assert gpu.bin_centers().shape == (N_BINS,)


@pytest.mark.parametrize("storage_dtype", [np.uint8, np.uint16, np.uint64,
                                           np.float32, np.float64])
def test_storage_dtype(frames, storage_dtype):
    gpu = make_gpu(np.uint16, storage_dtype=storage_dtype)
    gpu.fill_histogram(frames)
    values = gpu.values()
    assert values.dtype == storage_dtype
    np.testing.assert_array_equal(values, cpu_values(frames))


def test_registered_buffer_outlives_the_caller(frames):
    gpu = make_gpu(np.uint16)
    data = frames.copy()
    gpu.register_input_buffer(data)
    gpu.fill_histogram(data)
    del data  # kept alive by the registration
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))
    gpu.unregister_input_buffer()


def test_several_fills_accumulate(frames):
    gpu = make_gpu(np.uint16)
    gpu.fill_histogram(frames[:20])
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames[:20]))
    gpu.fill_histogram(frames[20:])
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))


def test_read_only_frames_are_accepted(frames):
    frames.setflags(write=False)
    gpu = make_gpu(np.uint16)
    gpu.fill_histogram(frames)
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))


def test_values_pixel(frames):
    gpu = make_gpu(np.uint16)
    gpu.fill_histogram(frames)
    values = gpu.values()
    for row, col in [(0, 0), (ROWS - 1, COLS - 1), (3, 6)]:
        np.testing.assert_array_equal(gpu.values_pixel(row, col),
                                      values[row, col])
    with pytest.raises(IndexError):
        gpu.values_pixel(ROWS, 0)


def test_values_into_out(frames):
    gpu = make_gpu(np.uint16)
    out = np.empty((ROWS, COLS, N_BINS), dtype=np.uint32)
    gpu.register_output_buffer(out)
    gpu.fill_histogram(frames[:20])
    assert gpu.values(out=out) is out
    np.testing.assert_array_equal(out, cpu_values(frames[:20]))
    gpu.fill_histogram(frames[20:])  # the same array again, after more frames
    gpu.values(out=out)
    np.testing.assert_array_equal(out, cpu_values(frames))
    gpu.unregister_output_buffer()


def test_registered_out_is_kept_alive():
    gpu = make_gpu(np.uint16)
    out = np.empty((ROWS, COLS, N_BINS), dtype=np.uint32)
    gpu.register_output_buffer(out)
    ref = weakref.ref(out)
    del out
    gc.collect()
    assert ref() is not None  # still pinned, so it must not be freed


def test_bad_out_is_rejected():
    gpu = make_gpu(np.uint16)
    good = np.empty((ROWS, COLS, N_BINS), dtype=np.uint32)
    with pytest.raises(TypeError, match="dtype uint32"):
        gpu.values(out=good.astype(np.uint16))
    with pytest.raises(TypeError, match="numpy array"):
        gpu.values(out=good.tolist())
    with pytest.raises(ValueError, match="shape"):
        gpu.values(out=good[:, :, :-1])
    with pytest.raises(ValueError, match="C-contiguous"):
        gpu.values(out=np.empty((N_BINS, COLS, ROWS), np.uint32).transpose())
    good.setflags(write=False)
    with pytest.raises(ValueError, match="writeable"):
        gpu.values(out=good)
    with pytest.raises(ValueError, match="shape"):
        gpu.register_output_buffer(np.empty((ROWS, COLS), np.uint32))


def test_wrong_dtype_is_not_converted(frames):
    gpu = make_gpu(np.uint16)
    with pytest.raises(TypeError, match="dtype=float64"):
        gpu.fill_histogram(frames.astype(np.float64))
    with pytest.raises(TypeError):
        gpu.register_input_buffer(frames.astype(np.float32))


def test_bad_layout_is_rejected(frames):
    gpu = make_gpu(np.uint16)
    with pytest.raises(ValueError, match="C-contiguous"):
        gpu.fill_histogram(frames[:, :, ::-1])
    with pytest.raises(ValueError, match="3D"):
        gpu.fill_histogram(frames[0])


def test_factory_rejects_unsupported_dtypes():
    with pytest.raises(ValueError, match="Unsupported dtype"):
        make_gpu(np.int32)
    with pytest.raises(ValueError, match="Unsupported storage_dtype"):
        make_gpu(np.uint16, storage_dtype=np.int8)


def test_constructor_validates_arguments():
    with pytest.raises(ValueError):
        PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMAX,
                           xmax=XMIN, dtype=np.uint16)
    with pytest.raises(ValueError):
        PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN,
                           xmax=XMAX, dtype=np.uint16, batch_size=0)
