# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import PedestalTrackingPixelHistogram, PixelHistogram_u16, read_state_header

SHAPE = (5, 7)
KWARGS = dict(rows=5, cols=7, n_bins=32, xmin=-16.0, xmax=16.0)


def _frame(offset):
    base = 100 + np.arange(SHAPE[0] * SHAPE[1]).reshape(SHAPE) % 5
    return (base + offset).astype(np.uint16)


def _seeded(**kwargs):
    """A histogram with 1000 pedestal frames of noise 2."""
    hist = PedestalTrackingPixelHistogram(**KWARGS, **kwargs)
    for i in range(1000):
        hist.push_pedestal_no_update(_frame(-2 if i % 2 == 0 else 2))
    hist.update_mean()
    return hist


def _fill(hist, first, last):
    # Inside and outside the gate, so that only some frames update the pedestal
    offsets = [1, -3, 8, 0, 3, -1]
    for i in range(first, last):
        hist.fill_async(_frame(offsets[i % 6]))


def _assert_same(actual, expected):
    np.testing.assert_array_equal(actual.values(), expected.values())
    np.testing.assert_array_equal(actual.pedestal_mean(), expected.pedestal_mean())
    np.testing.assert_array_equal(actual.pedestal_std(), expected.pedestal_std())
    assert actual.n_sigma == expected.n_sigma


def test_state_file_restores_the_histogram(tmp_path):
    mask = np.zeros(SHAPE, dtype=bool)
    mask[3, 4] = True
    original = _seeded(n_threads=2, n_sigma=2.0, mask=mask)
    _fill(original, 0, 9)
    # Pending fills are included
    assert original.save_state(tmp_path / "hist.aare") is None

    state = read_state_header(tmp_path / "hist.aare")["state"]
    assert state["n_sigma"] == 2.0
    assert state["pedestal"]["cur_samples"] == 1000
    assert state["histogram"]["values"]["shape"] == [5, 7, 32]
    assert state["mask"]["dtype"] == "|b1"

    # A new histogram with other threads, and one that holds other settings
    restored = PedestalTrackingPixelHistogram.from_state_file(
        tmp_path / "hist.aare", n_threads=3
    )
    given = PedestalTrackingPixelHistogram(**KWARGS, n_sigma=5.0)
    assert given.load_state(str(tmp_path / "hist.aare")) is None
    _assert_same(restored, original)

    for hist in (original, restored, given):
        _fill(hist, 9, 30)
    _assert_same(restored, original)
    _assert_same(given, original)
    assert not given.values()[3, 4].any()  # masked


def test_load_state_keeps_the_histogram_on_error(tmp_path):
    hist = _seeded(n_sigma=2.0)
    _fill(hist, 0, 6)
    reference = _seeded(n_sigma=2.0)
    _fill(reference, 0, 6)

    PedestalTrackingPixelHistogram(
        rows=5, cols=7, n_bins=31, xmin=-16.0, xmax=16.0
    ).save_state(tmp_path / "other_bins.aare")
    with pytest.raises(RuntimeError, match=r"expected \(5, 7, 32\)"):
        hist.load_state(tmp_path / "other_bins.aare")
    PixelHistogram_u16(**KWARGS).save_state(tmp_path / "plain.aare")
    with pytest.raises(RuntimeError, match="expected a PedestalTrackingPixelHistogram"):
        hist.load_state(tmp_path / "plain.aare")
    with pytest.raises(RuntimeError, match="expected a PedestalTrackingPixelHistogram"):
        PedestalTrackingPixelHistogram.from_state_file(tmp_path / "plain.aare")

    _assert_same(hist, reference)
