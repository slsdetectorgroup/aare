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
| `cf_common.py` | cluster-finder settings: pedestal frames, n_sigma, headers for the rebuild guard, `DEVICE_PED_TYPE` |
| `page_faults.py` | host side, no profiler: FPS and page faults of the output path, with and without the glibc heap setting and pre-heating |
| `../gpu_span.py` | shared: reads the SQLite export, computes busy time, duty cycle, roofline |
| `../common.py` | shared: paths, guards (fresh build, idle GPU), `env.json` stamp |

## Requirements

- A built tree with the CUDA extension: `build/aare/_aare_cuda*.so`. Run the
  scripts with a Python of the same minor version as the build, with numpy and
  matplotlib installed (`aare/__init__.py` imports both). A system Python
  often lacks matplotlib; a conda environment of the same version works.
- Nsight Systems. Default path `/opt/nvidia/nsight-systems/2024.5.1/bin/nsys`,
  override with `NSYS=...`.
- The MOENCH dataset under `/mnt/sls_det_storage/.../2026032408/process/xrf/`
  (override with `AARE_PERF_DATA=<dir>`). 1000 pedestal frames, 20 000 data
  frames, 5 sigma.
- An idle GPU. The driver aborts above 5 % utilisation: a competing process
  leaves per-op averages intact but ruins duty cycles and rooflines.

Paths are resolved relative to `python/tests/perf/`. `AARE_REPO` and `AARE_BUILD`
override the repo root and build directory.

## Run

```bash
cd python/tests/perf/cf
python run_probes.py                # full sweep: 4 configs, 20k frames each (~5 min)
python run_probes.py --frames 2000  # smoke test only; clocks do not ramp, kernel reads ~7 % slow
python run_probes.py --only 3x3_s1_uncontended 9x9_s1_uncontended --tag mychange
```

Configs:

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
python ../gpu_span.py results/<dir>/probe_9x9_s1_uncontended_cap1700.sqlite 20000
```

`probes.csv` and `env.json` are small and can be committed with a result;
the `.nsys-rep` and `.sqlite` traces are tens of MB each and are gitignored.

## Host memory and page faults

`page_faults.py` times the host side of the output path, without nsys: keeping
every `ClusterVector`, filling a spectrum from them, and the zero-copy views.
It also counts the minor page faults of each. Run each configuration in a fresh
process: the heap's state carries over from one measurement to the next.

```bash
python page_faults.py                       # glibc defaults
python page_faults.py --preheat-gb 3        # pre-heat, mostly given back without the setting
GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432:glibc.malloc.trim_threshold=4294967295:glibc.malloc.top_pad=268435456 \
    python page_faults.py --preheat-gb 3    # keep freed memory, then pre-heat
```

2026-11-19, RTX 4090 with free memory, 20 000 frames, 3x3, batch 3000:
keeping every `ClusterVector` runs at 31 000 FPS with the defaults (22.7
faults per frame), 41 100 with the setting (0.2 faults per frame), and 51 200
with the setting and a 3 GB pre-heat. The zero-copy path runs at about
56 700 FPS in all of them. `top_pad` alone gives the setting's gain on a fresh
start: the heap then grows in 256 MiB steps, which the kernel backs with 2 MiB
transparent huge pages. `trim_threshold` or `mmap_threshold` alone gain
nothing. The page `docs/src/python/cluster/pyClusterFinderCUDA.rst`
("Performance") explains the setting and how to put it in a Jupyter kernel.

Huge pages need free memory. With only 4 GB free (another process holding
65 GB), the same three runs gave 29 400, 30 100 and 45 800 FPS, and the
pre-heat took seconds per GB instead of about 0.1 s. Check `free -g` before
quoting a number. With huge pages one fault maps 2 MiB, so a low fault count
does not mean the memory came free: compare the times.

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

Kernel us/frame. "before" is the kernel before the refactor (an August commit
that was never pushed), rebuilt and re-probed the same day, matching its August
numbers: `results/2026-09-10_f32_ref7177/`.
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

A100 results, 3x3 to 9x9 at 1, 4 and 8 streams:
[`results/A100/README.md`](results/A100/README.md).
