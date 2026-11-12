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


def make_gpu(dtype, storage_dtype=np.uint32, batch_size=8, window=True,
             n_bins=N_BINS):
    return PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=n_bins, xmin=XMIN,
                              xmax=XMAX, dtype=dtype,
                              storage_dtype=storage_dtype,
                              batch_size=batch_size, window=window)


def cpu_values(frames, n_bins=N_BINS):
    cpu = PixelHistogram_u32(rows=ROWS, cols=COLS, n_bins=n_bins, xmin=XMIN,
                             xmax=XMAX)
    for frame in frames:
        cpu.fill_async(frame.astype(np.float64))
    return cpu.values()


@pytest.mark.parametrize("window", [True, False])
@pytest.mark.parametrize("dtype", [np.uint16, np.float32, np.float64])
def test_matches_cpu(frames, dtype, window):
    data = frames.astype(dtype)
    gpu = make_gpu(dtype, window=window)
    gpu.fill_histogram(data)
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))
    assert gpu.dtype == np.dtype(dtype)
    assert gpu.window is window


@pytest.mark.parametrize("window", [True, False])
def test_matches_cpu_with_photons_and_fine_bins(window):
    # 1200 bins of 1 ADU: more bins than the window holds. 5 % of values get
    # a photon far outside it, and the pedestals jump halfway.
    rng = np.random.default_rng(7)
    base = rng.uniform(4000, 4600, (ROWS, COLS))
    data = base + rng.normal(0, 10, (300, ROWS, COLS))
    data[150:] += 300
    data += (rng.random(data.shape) < 0.05) * rng.uniform(100, 200, data.shape)
    data = np.round(data).astype(np.uint16)
    gpu = make_gpu(np.uint16, batch_size=64, window=window, n_bins=1200)
    gpu.fill_histogram(data)
    np.testing.assert_array_equal(gpu.values(), cpu_values(data, n_bins=1200))


def test_defaults():
    # Checks the factory's own defaults.
    gpu = PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN,
                             xmax=XMAX, dtype=np.uint16)
    assert gpu.batch_size == 1024
    assert gpu.window is True



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
    gpu.unregister_input_buffer()  # releases the array kept alive above


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


def test_failed_call_does_not_fail_the_next_fill(frames):
    # CUDA refuses to register one array twice. The error it keeps until read
    # must not make a later fill raise after its work was queued: a retry
    # would count the frames twice.
    a, a2 = make_gpu(np.uint16), make_gpu(np.uint16)
    a.register_input_buffer(frames)
    with pytest.raises(RuntimeError, match="already mapped"):
        a2.register_input_buffer(frames)
    b = make_gpu(np.uint16)
    b.fill_histogram(frames)
    np.testing.assert_array_equal(b.values(), cpu_values(frames))


def test_read_only_memmap_can_be_registered(frames, tmp_path):
    path = tmp_path / "frames.raw"
    frames.tofile(path)
    mm = np.memmap(path, dtype=np.uint16, mode="r", shape=frames.shape)
    gpu = make_gpu(np.uint16)
    gpu.register_input_buffer(mm)  # pinned read-only
    gpu.fill_histogram(mm)
    np.testing.assert_array_equal(gpu.values(), cpu_values(frames))
    gpu.unregister_input_buffer()  # unpinning a read-only mapping works too


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
    # float('inf'), NaN, and 1e40, which is inf on the float32 axis.
    for xmin, xmax in [(XMIN, np.inf), (np.nan, XMAX), (XMIN, 1e40)]:
        with pytest.raises(ValueError, match="finite"):
            PixelHistogramCUDA(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=xmin,
                               xmax=xmax, dtype=np.uint16)
    # The window kernel counts a batch in uint16 shared counters.
    with pytest.raises(ValueError, match="65535"):
        make_gpu(np.uint16, batch_size=65536, window=True)
    make_gpu(np.uint16, batch_size=65536, window=False)
