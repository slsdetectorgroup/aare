"""The process nsys traces: fill one histogram with N frames, read it out, exit.

    nsys_hist_probe.py <dtype> <n_frames> <n_bins> <batch> <chunk>

dtype is u16, f32 or f64 (the frame dtype copied to the GPU). Run by
run_probes.py; run directly only under `nsys profile --trace=cuda`. The wall
times printed are profiler-inflated and are not throughput numbers.

Only `chunk` frames are read and held in host memory. They are registered once
and passed to fill_histogram() repeatedly until n_frames have been queued: the
histogram doesn't depend on frame order, and every pass is a real H2D copy.
The chunk is rounded down to a multiple of the batch size so that no pass ends
in a partial batch.
"""
import math
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hist_common  # noqa: E402,F401  (puts ../ and the build dir on sys.path)
from hist_common import common  # noqa: E402

from aare import File, PixelHistogramCUDA  # noqa: E402

DTYPES = {"u16": np.uint16, "f32": np.float32, "f64": np.float64}

dtype = DTYPES[sys.argv[1]]
N, n_bins, batch, chunk = (int(a) for a in sys.argv[2:6])
chunk = max(batch, min(chunk, N) // batch * batch)

f = File(common.DATA_FILE)
data = f.read_n(chunk)
# Range from the data itself, so that nearly every value is histogrammed.
lo, hi = np.percentile(data[:min(chunk, 200)], [0.1, 99.9])
xmin, xmax = float(math.floor(lo)), float(math.ceil(hi) + 1)
data = np.ascontiguousarray(data.astype(dtype, copy=False))
rows, cols = data.shape[1:]

hist = PixelHistogramCUDA(rows=rows, cols=cols, n_bins=n_bins, xmin=xmin,
                          xmax=xmax, dtype=data.dtype, storage_dtype=np.uint32,
                          batch_size=batch)
out = np.empty((rows, cols, n_bins), dtype=np.uint32)
hist.register_input_buffer(data)
hist.register_output_buffer(out)

t0 = time.perf_counter()
queued = 0
while queued < N:
    n = min(chunk, N - queued)
    hist.fill_histogram(data[:n])
    queued += n
hist.flush()
t_fill = time.perf_counter() - t0

t0 = time.perf_counter()
hist.values(out=out)
t_read = time.perf_counter() - t0
hist.unregister_input_buffer()
hist.unregister_output_buffer()

in_range = out.sum(dtype=np.uint64) / (N * rows * cols)
print(f"dtype={data.dtype}  N={N}  n_bins={n_bins}  batch={batch}  chunk={chunk}  "
      f"range=[{xmin:g}, {xmax:g})")
print(f"  H2D/frame={rows * cols * data.itemsize:,} B   "
      f"readout={out.nbytes:,} B   in range={100 * in_range:.2f}%")
print(f"  wall fill={t_fill:.3f}s ({N / t_fill:.0f} FPS)  readout={t_read * 1e3:.1f} ms  "
      f"(profiler-inflated)")
