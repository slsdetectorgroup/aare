# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import (PedestalTrackingPixelHistogram,
                  PedestalTrackingPixelHistogramCUDA, _cuda_available)

pytestmark = pytest.mark.skipif(
    not _cuda_available(), reason="aare built without AARE_CUDA")

# Non-square. The CPU class needs exactly 1000 seed frames.
ROWS, COLS, N_SEED, N_DATA = 7, 13, 1000, 300
N_BINS, XMIN, XMAX = 300, -50.0, 250.0


@pytest.fixture(scope="module")
def frames():
    """Seed frames, then data frames with a drift, a step and photons."""
    rng = np.random.default_rng(7)
    base = rng.uniform(1000, 3000, (ROWS, COLS))
    sigma = rng.uniform(2, 12, (ROWS, COLS))
    seed = base + sigma * rng.normal(size=(N_SEED, ROWS, COLS))
    data = base + sigma * rng.normal(size=(N_DATA, ROWS, COLS))
    data += np.linspace(0, 20, N_DATA)[:, None, None]
    data[N_DATA // 2:, ::3, ::2] += 40
    photons = rng.random(data.shape) < 0.05
    data[photons] += rng.uniform(30, 400, photons.sum())
    as_u16 = lambda a: np.clip(np.rint(a), 0, 65535).astype(np.uint16)
    return as_u16(seed), as_u16(data)


def run_gpu(seed, data, parts=(N_DATA,), **kwargs):
    hist = PedestalTrackingPixelHistogramCUDA(
        rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN, xmax=XMAX, **kwargs)
    hist.push_pedestal_no_update(seed)
    hist.update_mean()
    out, begin = [], 0
    for end in parts:
        hist.fill_histogram(data[begin:end])
        out.append((hist.values(), hist.pedestal_mean(), hist.pedestal_std()))
        hist.update_mean()
        begin = end
    return out


def run_cpu(seed, data, parts=(N_DATA,), n_sigma=1.0, mask=None):
    hist = PedestalTrackingPixelHistogram(
        rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN, xmax=XMAX,
        n_threads=3, n_sigma=n_sigma, mask=mask)
    for frame in seed:
        hist.push_pedestal_no_update(frame)
    hist.update_mean()
    out, begin = [], 0
    for end in parts:
        for frame in data[begin:end]:
            hist.fill_async(frame)
        hist.flush()
        out.append((hist.values(), hist.pedestal_mean(), hist.pedestal_std()))
        hist.update_mean()
        begin = end
    return out


def assert_equal(cpu, gpu):
    assert len(cpu) == len(gpu)
    for (cv, cm, cs), (gv, gm, gs) in zip(cpu, gpu):
        np.testing.assert_array_equal(gv, cv)
        np.testing.assert_array_equal(gm, cm)
        np.testing.assert_array_equal(gs, cs)


@pytest.mark.parametrize("window", [True, False])
@pytest.mark.parametrize("batch_size", [1, 64, 1024])
def test_equals_cpu(frames, window, batch_size):
    seed, data = frames
    parts = (100, 300)
    gpu = run_gpu(seed, data, parts, batch_size=batch_size, window=window)
    assert_equal(run_cpu(seed, data, parts), gpu)
    # The tracking moved the pedestal.
    assert not np.array_equal(gpu[-1][1], seed.mean(axis=0, dtype=np.float32))


@pytest.mark.parametrize("n_sigma", [0.0, 3.0])
def test_n_sigma(frames, n_sigma):
    seed, data = frames
    assert_equal(run_cpu(seed, data, n_sigma=n_sigma),
                 run_gpu(seed, data, n_sigma=n_sigma))


def test_mask(frames):
    seed, data = frames
    mask = np.zeros((ROWS, COLS), dtype=bool)
    mask[0, 0] = mask[3, 5] = True
    gpu = run_gpu(seed, data, mask=mask)
    assert_equal(run_cpu(seed, data, mask=mask), gpu)
    assert gpu[-1][0][3, 5].sum() == 0


def test_seed_frame_by_frame_and_errors(frames):
    seed, data = frames
    hist = PedestalTrackingPixelHistogramCUDA(
        rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN, xmax=XMAX,
        n_samples=10)
    assert hist.n_samples == 10
    with pytest.raises(RuntimeError, match="needs 10 frames, got 0"):
        hist.update_mean()
    with pytest.raises(RuntimeError, match="not seeded"):
        hist.pedestal_mean()
    np.testing.assert_array_equal(hist.pedestal_std(), 0)
    for frame in seed[:10]:
        hist.push_pedestal_no_update(frame)
    assert hist.pedestal_frames == 10
    with pytest.raises(RuntimeError, match="already has its 10 frames"):
        hist.push_pedestal_no_update(seed[0])
    np.testing.assert_array_equal(
        hist.pedestal_mean(),
        (seed[:10].astype(np.float64).sum(axis=0) * (1 / 10)).astype(
            np.float32))
    hist.update_mean()
    assert (hist.pedestal_std() > 0).all()

    with pytest.raises(TypeError, match="takes uint16 frames"):
        hist.fill_histogram(data.astype(np.float32))
    with pytest.raises(TypeError, match="takes uint16 frames"):
        hist.push_pedestal_no_update(seed[0].astype(np.int32))
    with pytest.raises(ValueError, match="shape"):
        hist.fill_histogram(np.ascontiguousarray(data[:, :, :-1]))
    with pytest.raises(ValueError, match="shape"):
        hist.push_pedestal_no_update(np.zeros((COLS, ROWS), np.uint16))


def test_constructor_checks():
    kw = dict(rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN, xmax=XMAX)
    with pytest.raises(ValueError, match="n_samples"):
        PedestalTrackingPixelHistogramCUDA(**kw, n_samples=0)
    with pytest.raises(ValueError, match="mask"):
        PedestalTrackingPixelHistogramCUDA(
            **kw, mask=np.zeros((COLS, ROWS), dtype=bool))
    with pytest.raises(TypeError, match="bool"):
        PedestalTrackingPixelHistogramCUDA(
            **kw, mask=np.zeros((ROWS, COLS), dtype=np.uint8))
    with pytest.raises(ValueError, match="Unsupported storage_dtype"):
        PedestalTrackingPixelHistogramCUDA(**kw, storage_dtype=np.float32)


def test_properties_and_storage(frames):
    seed, data = frames
    hist = PedestalTrackingPixelHistogramCUDA(
        rows=ROWS, cols=COLS, n_bins=N_BINS, xmin=XMIN, xmax=XMAX,
        storage_dtype=np.uint32, n_sigma=2.0, batch_size=50)
    assert (hist.rows, hist.cols, hist.n_bins, hist.batch_size) == (
        ROWS, COLS, N_BINS, 50)
    assert hist.n_sigma == 2.0
    hist.n_sigma = 3.0
    assert hist.n_sigma == 3.0
    assert hist.window
    np.testing.assert_array_equal(hist.bin_edges(),
                                  np.linspace(XMIN, XMAX, N_BINS + 1))
    hist.push_pedestal_no_update(seed)
    hist.update_mean()
    hist.register_input_buffer(data)
    hist.fill_histogram(data)
    out = np.empty((ROWS, COLS, N_BINS), dtype=np.uint32)
    assert hist.values(out=out) is out
    np.testing.assert_array_equal(hist.values_pixel(2, 3), out[2, 3])
    assert out.dtype == np.uint32
    hist.unregister_input_buffer()
