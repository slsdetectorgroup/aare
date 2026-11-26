# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import PixelHistogram, PixelHistogram_u16, PixelHistogram_u32, read_state_header


def _random_frames(rows, cols, n, xmin, xmax, seed=0):
    rng = np.random.default_rng(seed)
    return [rng.uniform(xmin - 0.25, xmax + 0.25, size=(rows, cols)).astype(np.float64)
            for _ in range(n)]


def _reference_hdata(frames, rows, cols, n_bins, xmin, xmax):
    expected = np.zeros((rows, cols, n_bins), dtype=np.uint16)
    inv_range = n_bins / (xmax - xmin)
    for img in frames:
        for r in range(rows):
            for c in range(cols):
                v = float(img[r, c])
                if not (xmin <= v < xmax):
                    continue
                b = int((v - xmin) * inv_range)
                if b >= n_bins:
                    b = n_bins - 1
                expected[r, c, b] += 1
    return expected


def test_async_fill_matches_reference():
    rows, cols, n_bins = 5, 7, 8
    xmin, xmax = 0.0, 2.0
    frames = _random_frames(rows, cols, n=3, xmin=xmin, xmax=xmax, seed=1)

    hist = PixelHistogram(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin, xmax=xmax)
    for img in frames:
        hist.fill_async(img)

    np.testing.assert_array_equal(
        hist.values(),
        _reference_hdata(frames, rows, cols, n_bins, xmin, xmax),
    )




def test_fill_async_copies_buffer():
    # After fill_async returns, the caller should be free to mutate the
    # numpy array without affecting the pending fill.
    rows, cols, n_bins = 4, 4, 4
    xmin, xmax = 0.0, 1.0
    hist = PixelHistogram(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin, xmax=xmax, n_threads=1, max_pending=8)

    img = np.full((rows, cols), 0.1, dtype=np.float64)  # falls in bin 0
    hist.fill_async(img)
    # Mutate the original array immediately; this must not affect the
    # value that was already enqueued.
    img[:] = 0.9  # would be bin 3
    hist.flush()

    h = hist.values()
    assert h.shape == (rows, cols, n_bins)
    # Every pixel saw one value in bin 0, none elsewhere.
    assert (h[:, :, 0] == 1).all()
    assert (h[:, :, 1:] == 0).all()


    def test_fill_async_rejects_wrong_shape():
        hist = PixelHistogram(8, 8, 4, 0.0, 1.0)
        bad = np.zeros((4, 4), dtype=np.float32)
        with pytest.raises(ValueError):
            hist.fill_async(bad)


def test_hdata_flushes_pending():
    # Submit several frames with a tiny queue and read hdata() without an
    # explicit flush(); hdata() must drain everything first.
    rows, cols, n_bins = 3, 3, 4
    xmin, xmax = 0.0, 1.0
    hist = PixelHistogram(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin, xmax=xmax,
                          n_threads=1, max_pending=1)
    frames = _random_frames(rows, cols, n=8, xmin=xmin, xmax=xmax, seed=3)
    for img in frames:
        hist.fill_async(img)

    h = hist.values()  # no explicit flush()
    np.testing.assert_array_equal(
        h, _reference_hdata(frames, rows, cols, n_bins, xmin, xmax)
    )


def test_bin_centers_and_edges():
    n_bins = 5
    xmin, xmax = 0.0, 1.0
    hist = PixelHistogram(rows=2, cols=2, n_bins=n_bins, xmin=xmin, xmax=xmax)
    edges = hist.bin_edges()
    centers = hist.bin_centers()
    assert edges.shape == (n_bins + 1,)
    assert centers.shape == (n_bins,)
    np.testing.assert_allclose(edges, np.linspace(xmin, xmax, n_bins + 1), atol=1e-6)
    np.testing.assert_allclose(centers, 0.5 * (edges[:-1] + edges[1:]), atol=1e-6)


def test_state_file_restores_the_counts(tmp_path):
    kwargs = dict(rows=6, cols=5, n_bins=8, xmin=0.0, xmax=4.0)
    frames = _random_frames(6, 5, 12, 0.0, 4.0)
    original = PixelHistogram_u16(n_threads=3, **kwargs)
    for frame in frames[:6]:
        original.fill_async(frame)
    # Pending fills are included
    assert original.save_state(tmp_path / "hist.aare") is None

    header = read_state_header(tmp_path / "hist.aare")
    assert header["type"] == "PixelHistogram"
    assert header["state"]["values"]["dtype"] == "<u2"
    assert header["state"]["values"]["shape"] == [6, 5, 8]

    # A new histogram, and one that holds other counts
    restored = PixelHistogram_u16.from_state_file(tmp_path / "hist.aare", n_threads=2)
    given = PixelHistogram_u16(**kwargs)
    given.fill_async(frames[-1])
    assert given.load_state(str(tmp_path / "hist.aare")) is None
    np.testing.assert_array_equal(restored.values(), original.values())
    np.testing.assert_array_equal(restored.bin_edges(), original.bin_edges())

    for frame in frames[6:]:
        for hist in (original, restored, given):
            hist.fill_async(frame)
    expected = _reference_hdata(frames, 6, 5, 8, 0.0, 4.0)
    for hist in (original, restored, given):
        np.testing.assert_array_equal(hist.values(), expected)


def test_load_state_keeps_the_histogram_on_error(tmp_path):
    hist = PixelHistogram_u16(rows=6, cols=5, n_bins=8, xmin=0.0, xmax=4.0)
    hist.fill_async(_random_frames(6, 5, 1, 0.0, 4.0)[0])
    before = hist.values()

    PixelHistogram_u16(rows=6, cols=5, n_bins=9, xmin=0.0, xmax=4.0).save_state(
        tmp_path / "other_bins.aare"
    )
    with pytest.raises(RuntimeError, match=r"expected \(6, 5, 8\)"):
        hist.load_state(tmp_path / "other_bins.aare")
    # The counts are not converted to another storage dtype
    PixelHistogram_u32(rows=6, cols=5, n_bins=8, xmin=0.0, xmax=4.0).save_state(
        tmp_path / "u32.aare"
    )
    with pytest.raises(RuntimeError, match="has dtype <u4, expected <u2"):
        hist.load_state(tmp_path / "u32.aare")
    with pytest.raises(RuntimeError, match="has dtype <u4, expected <u2"):
        PixelHistogram_u16.from_state_file(tmp_path / "u32.aare")

    np.testing.assert_array_equal(hist.values(), before)
