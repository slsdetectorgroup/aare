# GPU performance probes

Nsight Systems probes for the CUDA classes: per-engine GPU time per frame
(kernel, H2D, D2H), duty cycles and the roofline. One folder per class, each
with its own driver, traced process, README and `results/`:

| folder | class |
|---|---|
| [`cf/`](cf/README.md) | `ClusterFinderCUDA` |
| [`hist/`](hist/README.md) | `PixelHistogramCUDA` |

Shared by all of them:

| file | role |
|---|---|
| `common.py` | paths (`AARE_REPO`, `AARE_BUILD`, `AARE_PERF_DATA`, `NSYS`), guards (fresh build, idle GPU), `env.json` stamp, results folder |
| `gpu_span.py` | reads an nsys SQLite export: busy time, duty cycle and roofline per engine, optionally for one kernel name and a subset of engines only |

Requirements (a CUDA build of the Python extension, Nsight Systems, the
dataset, an idle GPU) are listed in each folder's README.
