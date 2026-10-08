# ClusterFinderCUDA nsys probes

Per-engine GPU time per frame (kernel, H2D, D2H) for the CUDA cluster finder,
traced with Nsight Systems on real MOENCH data. The tallest engine is the
roofline: the engines overlap and PCIe is full-duplex, so throughput is bound by
the max, not the sum.

These are *not* throughput numbers. nsys inflates wall clock several-fold by
tracing every CUDA call. Use them to answer "what does the kernel cost" and
"which engine binds", and measure sustained FPS separately without a profiler.

## Files

| file | role |
|---|---|
| `run_probes.py` | driver: runs each config under nsys, exports SQLite, writes `probes.csv` |
| `nsys_kernel_probe.py` | the traced process: pedestal, one batched pass, exit |
| `gpu_span.py` | reads the SQLite export, computes busy time, duty cycle, roofline |
| `common.py` | paths, guards (fresh build, idle GPU), `env.json` stamp |

## Requirements

- A built tree with the CUDA extension: `build/aare/_aare_cuda*.so`. Run the
  scripts with a Python of the same minor version as the build, with numpy and
  matplotlib installed (`aare/__init__.py` imports both). On pc-moench-04 the
  build is 3.11 and the `py` conda environment works:
  `/home/ferjao_k/.conda/envs/py/bin/python`.
- Nsight Systems. Default path `/opt/nvidia/nsight-systems/2024.5.1/bin/nsys`,
  override with `NSYS=...`.
- The MOENCH dataset under `/mnt/sls_det_storage/.../2026032408/process/xrf/`
  (override with `AARE_PERF_DATA=<dir>`). 1000 pedestal frames, 20 000 data
  frames, 5 sigma.
- An idle GPU. The driver aborts above 5 % utilisation: a competing process
  leaves per-op averages intact but ruins duty cycles and rooflines.

Paths are resolved relative to this folder. `AARE_REPO` and `AARE_BUILD`
override the repo root and build directory.

## Run

```bash
cd python/tests/perf
python run_probes.py                # full sweep: 4 configs, 20k frames each (~5 min)
python run_probes.py --frames 2000  # smoke test only; clocks do not ramp, kernel reads ~7 % slow
python run_probes.py --only 3x3_s1_uncontended 9x9_s1_uncontended --tag mychange
```

Configs, matching the campaign in `docs/ClusterFinderCUDA_benchmark_results.md`
on branch `bench/cuda_cf`:

| label | cluster | cap | streams | batch | frames |
|---|---|---|---|---|---|
| `3x3_s4` | 3x3 | 3000 | 4 | 2000 | 20 000 |
| `3x3_s1_uncontended` | 3x3 | 3000 | 1 | 2000 | 20 000 |
| `9x9_s4` | 9x9 | 1700 | 4 | 2000 | 20 000 |
| `9x9_s1_uncontended` | 9x9 | 1700 | 1 | 2000 | 20 000 |

Keep the cap: at 9x9 the D2H slot is `4 + cap * sizeof(cluster)` and is copied
whole, so the cap sets the D2H bar.

Output goes to `results/<date>_<f32|f64>[_tag]/`: `env.json` (git rev, dirty
flag, driver, pedestal type), one `.nsys-rep` + `.sqlite` per config, and
`probes.csv`. Re-running one config replaces its own row and keeps the others.

To inspect one trace by hand:

```bash
nsys stats --report cuda_gpu_sum results/<dir>/probe_9x9_s1_uncontended_cap1700.nsys-rep
python gpu_span.py results/<dir>/probe_9x9_s1_uncontended_cap1700.sqlite 20000
```

`probes.csv` and `env.json` are small and can be committed with a result;
the `.nsys-rep` and `.sqlite` traces are tens of MB each and are gitignored.

## Reading the numbers

- **Quote kernel changes from the `s1_uncontended` rows.** With one stream
  nothing overlaps, so the kernel column is the kernel alone. The 1-stream
  kernel and transfer numbers reproduce across sessions to about 1 %.
- **4-stream transfer columns drift by up to 20 % between sessions** on an
  identical build (H2D and D2H fight for the PCIe link). Only compare s4
  rows taken in the same session, and read them for the bottleneck, not for
  a percentage.
- A kernel below the tallest transfer bar is invisible end to end. Further
  kernel work only pays once it is the tallest bar.

## Reference, 2026-09-10, RTX 4090, driver 595.71.05, float pedestal

Kernel us/frame. "before" is commit 7177f00 (pre-refactor kernel, rebuilt and
re-probed the same day, matching its August numbers): `results/2026-09-10_f32_ref7177/`.
"after" is the kernel with the cooperative tile load, size-dependent unroll
and two-phase load: `results/2026-09-10_f32/`.

| config | before | after | roofline after | binds |
|---|--:|--:|--:|---|
| 3x3, 1 stream | 4.32 | 4.65 | 13.13 | H2D |
| 3x3, 4 streams | 5.00 | 5.64 | 16.35 | H2D |
| 9x9, 1 stream | 23.80 | 15.29 | 22.00 | D2H |
| 9x9, 4 streams | 24.72 | 15.59 | 25.39 | D2H |

Both sizes are transfer-bound; sustained throughput is unchanged (3x3 about
61 400 FPS, 9x9 about 39 600 FPS on the zero-copy path).
