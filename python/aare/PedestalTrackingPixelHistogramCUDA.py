# SPDX-License-Identifier: MPL-2.0
import numpy as np

from . import _aare
from .ClusterFinder import _cuda_available

# numpy dtype of the counts -> class name suffix
_STORAGE_SUFFIX = {
    np.dtype(np.uint16): "u16",
    np.dtype(np.uint32): "u32",
}


def PedestalTrackingPixelHistogramCUDA(*, rows, cols, n_bins, xmin, xmax,
                                       n_sigma=1.0, mask=None,
                                       n_samples=1000,
                                       storage_dtype=np.uint16,
                                       batch_size=1024, window=True):
    """
    Per-pixel histograms of pedestal-subtracted frames, with the pedestal
    tracked during the fill, on the GPU: the GPU port of
    PedestalTrackingPixelHistogram. With n_samples=1000 the histograms,
    pedestal_mean() and pedestal_std() equal the CPU class's on the same
    frames.

    1. push_pedestal_no_update() with exactly n_samples raw frames seeds the
       pedestal.
    2. update_mean() sets each pixel's sigma from its pedestal. It stays
       fixed until the next update_mean() call.
    3. fill_histogram() histograms raw - mean for every value and, for a
       value with abs(raw - mean) < n_sigma * sigma, updates the pixel's
       pedestal with raw (a moving average with weight 1 / n_samples),
       frame by frame.

    Frames are uint16. The other methods (fill_histogram, values, the
    buffer registration, ...) are those of PixelHistogramCUDA. Not
    thread-safe.

    Parameters
    ----------
    rows, cols : int
        Frame shape.
    n_bins : int
        Number of bins per pixel.
    xmin, xmax : float
        Range of the residual raw - pedestal, [xmin, xmax). Values outside
        it are not counted, but still update the pedestal when within the
        gate.
    n_sigma : float, optional
        Gate of the pedestal update. 0 or less: no tracking. Default 1.0,
        as the CPU class. Also the n_sigma property.
    mask : numpy array of bool, shape (rows, cols), optional
        True for pixels to skip entirely: no histogram, no tracking.
    n_samples : int, optional
        Seed frames, and the weight 1 / n_samples of a value in the update.
        Default 1000; the CPU class has only 1000.
    storage_dtype : numpy dtype, optional
        dtype of the counts returned by values(): np.uint16 (default, as the
        CPU class) or np.uint32. Counts are capped at the type's maximum.
    batch_size, window : optional
        As for PixelHistogramCUDA.

    Example
    -------
    .. code-block:: python

        from aare import File, PedestalTrackingPixelHistogramCUDA

        pedestal = File(pedestal_path).read_n(1000)   # uint16
        frames = File(data_path).read_n(100_000)
        hist = PedestalTrackingPixelHistogramCUDA(
            rows=400, cols=400, n_bins=500, xmin=-100, xmax=400)
        hist.push_pedestal_no_update(pedestal)
        hist.update_mean()
        hist.register_input_buffer(frames)  # optional: faster copies
        hist.fill_histogram(frames)
        counts = hist.values()              # (400, 400, 500)
        mean = hist.pedestal_mean()         # the tracked pedestal
    """
    if not _cuda_available():
        raise RuntimeError(
            "PedestalTrackingPixelHistogramCUDA is not available in this "
            "build of aare. Rebuild with -DAARE_CUDA=ON (and "
            "-DAARE_PYTHON_BINDINGS=ON)."
        )
    try:
        suffix = _STORAGE_SUFFIX[np.dtype(storage_dtype)]
    except (KeyError, TypeError):
        supported = ", ".join(str(d) for d in _STORAGE_SUFFIX)
        raise ValueError(
            f"Unsupported storage_dtype: {storage_dtype}. "
            f"Supported: {supported}."
        ) from None
    cls = getattr(_aare, f"PedestalTrackingPixelHistogramCUDA_{suffix}")
    return cls(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin, xmax=xmax,
               n_sigma=n_sigma, mask=mask, n_samples=n_samples,
               batch_size=batch_size, window=window)
