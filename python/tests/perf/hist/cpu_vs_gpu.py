"""Fill per-pixel histograms with PixelHistogram (CPU) and PixelHistogramCUDA
(GPU) from the same frames, time both and check that the results are identical.

    cpu_vs_gpu.py [--n-frames N] [--n-bins B] [--xmin X] [--xmax X]
                  [--threads T] [--batch S] [--gpu-dtype {u16,f32,f64} ...]
                  [--kernel {window,direct}] [--csv PATH]

Script version of python/tests/PixelHistogramCUDA.ipynb, for a GPU node
without Jupyter. Not an nsys probe: the times are wall-clock.

The CPU binding takes only float64 frames, so the frames are converted once
before either timer starts. The conversion time is printed separately. The
CPU runs once; the GPU then runs once per --gpu-dtype (default u16, its
natural input), each compared with the same CPU histogram.

--csv appends one row per GPU run (header written if the file is new), with
the columns of the status dashboard's run table.

Peak host memory, 20 000 frames of 400 x 400: 6.4 GB of uint16 frames,
25.6 GB of float64 frames (kept until the GPU runs end when f64 is asked for),
6.4 GB more for f32 frames, and 3 histograms of 20000 x 4 x n_bins bytes
(1.28 GB each at 2000 bins, 2.56 GB at 4000). Ask for about 64 GB.

Exits with status 1 if any GPU histogram differs from the CPU's.
"""
import argparse
import csv
import os
import socket
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hist_common  # noqa: E402  (puts ../ and the build dir on sys.path)
from hist_common import common  # noqa: E402

from aare import File, PixelHistogram_u32, PixelHistogramCUDA  # noqa: E402

DTYPES = {"u16": np.uint16, "f32": np.float32, "f64": np.float64}
CSV_COLUMNS = ["node", "cpus", "cpu_threads", "cpu_s", "gpu_input",
               "gpu_batch", "gpu_fill_s", "gpu_values_s", "n_frames",
               "n_bins", "range_adu", "identical", "gpu_kernel"]

ap = argparse.ArgumentParser()
ap.add_argument("--n-frames", type=int, default=20_000)
ap.add_argument("--n-bins", type=int, default=2000)
ap.add_argument("--xmin", type=float, default=3500)
ap.add_argument("--xmax", type=float, default=7500)
ap.add_argument("--threads", type=int, default=len(os.sched_getaffinity(0)),
                help="CPU fill threads (default: the CPUs this job may use)")
ap.add_argument("--batch", type=int, default=1024, help="GPU batch size")
ap.add_argument("--gpu-dtype", choices=DTYPES, nargs="+", default=["u16"],
                help="one GPU run per dtype, e.g. --gpu-dtype u16 f32 f64")
ap.add_argument("--kernel", choices=["window", "direct"], default="window",
                help="GPU fill kernel: fill_window (default) or fill_direct")
ap.add_argument("--csv", type=Path, help="append one row per GPU run here")
args = ap.parse_args()

common.assert_build_fresh(hist_common.HEADERS)
print(common._sh("nvidia-smi --query-gpu=name,memory.used,memory.total,"
                 "utilization.gpu --format=csv,noheader"))

# ---- frames ----------------------------------------------------------------
t0 = time.perf_counter()
frames = File(common.DATA_FILE).read_n(args.n_frames)  # (n, rows, cols) uint16
n_frames, rows, cols = frames.shape
print(f"read {frames.shape} {frames.dtype}, {frames.nbytes / 1e9:.2f} GB "
      f"in {time.perf_counter() - t0:.1f} s")
print(f"{args.n_bins} bins over [{args.xmin}, {args.xmax}), "
      f"{(args.xmax - args.xmin) / args.n_bins:.2f} ADU per bin")

t0 = time.perf_counter()
frames64 = np.ascontiguousarray(frames, dtype=np.float64)
print(f"uint16 -> float64 conversion: {time.perf_counter() - t0:.2f} s "
      f"(not in the CPU time)")

# ---- CPU -------------------------------------------------------------------
cpu = PixelHistogram_u32(rows=rows, cols=cols, n_bins=args.n_bins,
                         xmin=args.xmin, xmax=args.xmax, n_threads=args.threads)
t0 = time.perf_counter()
for frame in frames64:
    cpu.fill_async(frame)
values_cpu = cpu.values()  # flushes the queue first
t_cpu = time.perf_counter() - t0
print(f"CPU ({args.threads} threads): {t_cpu:.2f} s, "
      f"{n_frames / t_cpu:,.0f} frames/s")
del cpu
if "f64" not in args.gpu_dtype:
    del frames64  # free 25.6 GB before the GPU runs

# ---- GPU, once per input dtype ---------------------------------------------
values_gpu = np.empty((rows, cols, args.n_bins), dtype=np.uint32)
csv_rows, all_identical = [], True

for name in args.gpu_dtype:
    gpu_dtype = DTYPES[name]
    if gpu_dtype is np.float64:
        frames_gpu = frames64
    elif gpu_dtype is np.uint16:
        frames_gpu = frames
    else:
        frames_gpu = np.ascontiguousarray(frames, dtype=gpu_dtype)

    gpu = PixelHistogramCUDA(rows=rows, cols=cols, n_bins=args.n_bins,
                             xmin=args.xmin, xmax=args.xmax, dtype=gpu_dtype,
                             storage_dtype=np.uint32, batch_size=args.batch,
                             window=args.kernel == "window")
    gpu.register_input_buffer(frames_gpu)  # pin once, outside the timers
    gpu.register_output_buffer(values_gpu)

    t0 = time.perf_counter()
    gpu.fill_histogram(frames_gpu)
    gpu.flush()  # fill_histogram only queues the work
    t1 = time.perf_counter()
    gpu.values(out=values_gpu)  # cap + copy back to the host
    t2 = time.perf_counter()
    t_fill, t_values, t_gpu = t1 - t0, t2 - t1, t2 - t0

    # Free device memory and unpin before the next dtype.
    del gpu
    if gpu_dtype is np.float32:
        del frames_gpu

    identical = np.array_equal(values_cpu, values_gpu)
    all_identical &= identical
    print(f"GPU {name} ({args.kernel}): fill {t_fill:.3f} s, {n_frames / t_fill:,.0f} frames/s, "
          f"{t_fill / n_frames * 1e6:.1f} us/frame | values() {t_values:.3f} s | "
          f"total {t_gpu:.3f} s, speed-up {t_cpu / t_gpu:.1f}x | "
          f"identical: {identical}")
    if not identical:
        diff = np.argwhere(values_cpu != values_gpu)
        print(f"  {len(diff):,} cells differ, first: {diff[:5].tolist()}; "
              f"counts CPU {values_cpu.sum():,} GPU {values_gpu.sum():,}")

    csv_rows.append(dict(
        node=socket.gethostname().split(".")[0],
        cpus=len(os.sched_getaffinity(0)), cpu_threads=args.threads,
        cpu_s=f"{t_cpu:.2f}", gpu_input=name, gpu_batch=args.batch,
        gpu_fill_s=f"{t_fill:.3f}", gpu_values_s=f"{t_values:.3f}",
        n_frames=n_frames, n_bins=args.n_bins,
        range_adu=f"{args.xmax - args.xmin:g}",
        identical=str(identical).lower(), gpu_kernel=args.kernel))

if args.csv:
    new = not args.csv.exists()
    with args.csv.open("a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_COLUMNS, lineterminator="\n")
        if new:
            w.writeheader()
        w.writerows(csv_rows)
    print(f"appended {len(csv_rows)} rows to {args.csv}")

sys.exit(0 if all_identical else 1)
