# SPDX-License-Identifier: MPL-2.0
import numpy as np

from . import _aare
from .ClusterFinder import _cuda_available

# numpy dtype -> class name suffix, for the frames and for the counts.
_INPUT_SUFFIX = {
    np.dtype(np.uint16): "u16",
    np.dtype(np.float32): "f",
    np.dtype(np.float64): "d",
}
_STORAGE_SUFFIX = {
    np.dtype(np.uint8): "u8",
    np.dtype(np.uint16): "u16",
    np.dtype(np.uint32): "u32",
    np.dtype(np.uint64): "u64",
    np.dtype(np.float32): "f",
    np.dtype(np.float64): "d",
}


def _suffix(table, dtype, what):
    try:
        return table[np.dtype(dtype)]
    except (KeyError, TypeError):
        supported = ", ".join(str(d) for d in table)
        raise ValueError(
            f"Unsupported {what}: {dtype}. Supported: {supported}."
        ) from None


def PixelHistogramCUDA(*, rows, cols, n_bins, xmin, xmax, dtype,
                       storage_dtype=np.uint32, batch_size=1024, window=True):
    """
    Per-pixel histograms filled on the GPU.

    The counts equal those of the C++ PixelHistogram with a float32 axis for
    uint16 and float32 frames, and a float64 axis for float64 frames. The
    Python PixelHistogram classes bin on a float64 axis, so with uint16 or
    float32 frames a value lying on a bin edge can land in the neighbouring
    bin.

    Not thread-safe: fill_histogram() and values() release the GIL, so don't
    use one object from two threads at once.

    Parameters
    ----------
    rows, cols : int
        Frame shape.
    n_bins : int
        Number of bins per pixel.
    xmin, xmax : float
        Histogram range, [xmin, xmax). Values outside it, and NaN, are
        dropped.
    dtype : numpy dtype
        dtype of the frames you will pass: np.uint16, np.float32 or
        np.float64. Frames are copied to the GPU as they are and are never
        converted, so fill_histogram() raises TypeError for any other dtype.
        The copy usually limits the speed: uint16 frames move half the bytes
        of float32 and a quarter of float64. Use float32 rather than float64
        for processed frames when its precision is enough.
    storage_dtype : numpy dtype, optional
        dtype of the counts returned by values(): np.uint8, np.uint16,
        np.uint32, np.uint64, np.float32 or np.float64. Integer counts are
        capped at the type's maximum. Default np.uint32.
    batch_size : int, optional
        Frames copied to the GPU and histogrammed per step. Two batches are
        held on the GPU: 2 * batch_size * rows * cols * itemsize bytes.
        For example 655 MB for 400 x 400 uint16 frames, but about 17 GB for
        1M-pixel float64 frames: lower batch_size for large frames.
        Default 1024; with window=True, at most 65535. The copy of one batch
        overlaps the kernel on the previous one, and larger batches shorten
        the kernel's time per frame, not the copy's. With 1024, the kernel
        needs under half the copy time, so the copy sets the speed; with 64,
        the kernel can take as long as the copy.
    window : bool, optional
        Fill kernel. True (default): during a batch, each pixel's 128
        consecutive bins around its noise peak are counted in fast on-chip
        memory, and only values outside them (mostly photons) go straight to
        the histogram in GPU memory. False: every value goes straight to GPU
        memory. The counts are identical; only the speed differs.

    Example
    -------
    .. code-block:: python

        import numpy as np
        from aare import PixelHistogramCUDA

        frames = np.fromfile(path, dtype=np.uint16).reshape(-1, 400, 400)
        hist = PixelHistogramCUDA(rows=400, cols=400, n_bins=1000,
                                  xmin=-0.5, xmax=999.5, dtype=frames.dtype)
        hist.register_input_buffer(frames)  # optional: faster copies;
                                            # one histogram per array
        hist.fill_histogram(frames)
        counts = hist.values()              # (400, 400, 1000)
    """
    if not _cuda_available():
        raise RuntimeError(
            "PixelHistogramCUDA is not available in this build of aare. "
            "Rebuild with -DAARE_CUDA=ON (and -DAARE_PYTHON_BINDINGS=ON)."
        )
    name = (f"PixelHistogramCUDA_{_suffix(_INPUT_SUFFIX, dtype, 'dtype')}_"
            f"{_suffix(_STORAGE_SUFFIX, storage_dtype, 'storage_dtype')}")
    cls = getattr(_aare, name)
    return cls(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin, xmax=xmax,
               batch_size=batch_size, window=window)
