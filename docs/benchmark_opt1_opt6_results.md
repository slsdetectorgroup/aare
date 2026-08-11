# ClusterFinderCUDA — Benchmark Results, opt1 → opt6

Consolidated, verified performance numbers for the CUDA cluster-finder optimization
ladder. Every number below is traced to the code, notebook cell, or profiler report
that produced it, and each is tagged **quotable** or **not quotable** with the reason.

Companion documents:
- `docs/pedestal_precision_f32_cancellation.md` — why the naive f32 pedestal failed and how B1 fixes it
- `docs/cuda_optimization_recap.md`, `docs/optimization_summary.md` — earlier narrative/roadmap notes
- `docs/ClusterFinderCUDA_optimizations.pptx` — the deck these numbers feed

---

## 1. Environment

| item | value |
|---|---|
| GPU | NVIDIA GeForce RTX 4090 (Ada, sm_89), 24 GB, driver 595.71.05 |
| GPU clocks | idle 210 MHz → boost 3120 MHz; **persistence mode disabled** |
| FP64 rate | 1/64 of FP32 on this part (relevant to opt6) |
| CPU | AMD Ryzen 9 7950X, 16 cores / 32 threads |
| RAM | 125 GiB, **no swap** |
| CUDA | 12.4 (nvcc V12.4.131) |
| Profiler | Nsight Systems 2024.5.1 (`/opt/nvidia/nsight-systems/2024.5.1`) |
| Host | `pc-moench-04` |
| Branch | `bench/opt2-pipeline` (off `feature/cuda_clusterfinder` @ `ce256dd`) |

**Dataset** — MOENCH, MAX IV beamtime, Cu fluorescence:

```
/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/
    Cu_factor_10_data_master_0.json        (100 000 frames, 400×400 uint16)
    Cu_factor_10_pedestal_master_0.json    (1 000 frames used for pedestal training)
```

Frame size 400×400×2 B = 320 000 B (312.5 KiB). All finders trained on the
**same 1 000 pedestal frames**;
`n_sigma = 5` throughout. Data pre-loaded into RAM with `read_n()` so file I/O is
outside every timing loop.

---

## 2. The optimization ladder

| step | what changed | how it is measured |
|---|---|---|
| **baseline** | `ClusterFinderMT`, 48 threads | `ClusterFinderMT(..., n_threads=48)` + `ClusterCollector` |
| **opt1** | first CUDA port: 1 stream, one launch per frame, no batching | `ClusterFinderCUDAOpt2(..., n_streams=1)` + `find_clusters()` per frame |
| **opt2** | multi-stream scaffolding + host-side batching (bulk memcpy) | `ClusterFinderCUDAOpt2(..., n_streams=4)` + `find_clusters_batched()`, batch 2000 |
| **opt3** | pipeline rework: remove per-round sync barriers, fixed-size D2H | `ClusterFinderCUDA(..., n_streams=4)` + `find_clusters_batched()`, **no pinning** |
| **opt4** | DMA-speed transfers via pinned host input | opt3 + `register_input_buffer(data)` |
| **opt5** | CUDA Graphs (pre-recorded H2D→kernel→D2H per stream) | `ClusterFinderCUDAGraph(...)` + pinned input |
| **opt6** | **first kernel optimization**: f32 device pedestal + variance rewrite (B1) | rebuild with `DEVICE_PED_TYPE = float` |

opt1–opt5 are **pipeline/host-side**; opt6 is the first change to the **kernel itself**.

### Code behind each step

| step | primary source |
|---|---|
| opt1, opt2 | [`include/aare/ClusterFinderCUDAOpt2.hpp`](../include/aare/ClusterFinderCUDAOpt2.hpp), [`include/aare/clusterfinder_kernel_opt2.cuh`](../include/aare/clusterfinder_kernel_opt2.cuh) — snapshot of commit `88e0e8d` (pre-refactor pipeline), namespace `aare::device_opt2` |
| opt3, opt4, opt6 | [`include/aare/ClusterFinderCUDA.hpp`](../include/aare/ClusterFinderCUDA.hpp), [`include/aare/clusterfinder_kernel.cuh`](../include/aare/clusterfinder_kernel.cuh) |
| opt5 | [`include/aare/ClusterFinderCUDA_graph.hpp`](../include/aare/ClusterFinderCUDA_graph.hpp) |
| bindings | [`python/src/bind_ClusterFinderCUDAOpt2.hpp`](../python/src/bind_ClusterFinderCUDAOpt2.hpp), [`python/src/cuda_bindings.cu`](../python/src/cuda_bindings.cu) |
| factories | [`python/aare/ClusterFinder.py`](../python/aare/ClusterFinder.py) |

Relevant history: `3ed773e` (multi-stream+batched) → `ac96d1f` (mixed precision) →
`88e0e8d` (**opt1/opt2 snapshot**) → `6a12e3d` (pipeline refactor = opt3) →
`4c66802` (FP32 pedestal, introduced the tail) → `5922c73` (async API) →
`1bf317f` (local-max fix) → `a42d71c` (graphs = opt5).

### Precision configuration

Two type aliases in [`clusterfinder_kernel.cuh:16-17`](../include/aare/clusterfinder_kernel.cuh#L16-L17):

```cpp
using COMPUTE_TYPE    = float;   // stencil arithmetic — float in ALL builds below
using DEVICE_PED_TYPE = double;  // device pedestal — the opt6 knob (double → float)
```

- **"f64 build"** in this document = `COMPUTE_TYPE=float`, `DEVICE_PED_TYPE=double` (mixed precision).
- **"f32 build"** (opt6) = `COMPUTE_TYPE=float`, `DEVICE_PED_TYPE=float` (100 % f32).

> ⚠️ `ClusterFinderCUDAOpt2` is templated on `PEDESTAL_TYPE`, which its binding pins
> to `double` ([`bind_ClusterFinderCUDAOpt2.hpp:14`](../python/src/bind_ClusterFinderCUDAOpt2.hpp#L14)).
> **opt1 and opt2 are therefore unaffected by the opt6 flip** — their numbers are
> identical in both campaigns by construction. Only opt3/opt4/opt5 respond to opt6.

---

## 3. Methodology — what is measurable and what is not

Three measurement artifacts were identified and controlled. **All three matter for
how numbers may be quoted on a slide.**

### 3.1 First-run soft page faults (dominant, ~1.5–4 s per cell)

Each timed cell materializes ~233 M clusters ≈ **10 GB of host result heap**. On the
first execution in a process, every 4 kB page must be faulted in and zeroed by the OS
(millions of minor faults). Measured cost on this machine: **0.7 µs/fault**.

Instrumentation added to all seven timed notebook cells:

```python
import resource
def _faults():
    r = resource.getrusage(resource.RUSAGE_SELF)
    return r.ru_minflt, r.ru_majflt
```
bracketing the timed region, printing `minor faults: N (~N×0.7µs est.)`.

**Protocol: quote the run where the fault counter has plateaued** (< ~200 k). Cold
numbers are inflated by 25–45 %. Verified by direct correlation (§9).

### 3.2 CUDA-event `kernel_ms` inflates under multi-stream saturation

`avg_kernel_time_ms()` uses a CUDA event pair on the kernel's own stream. It measures
**elapsed time on that stream's timeline**, which includes queue-wait when other
streams are competing for SMs. Consequences:

- In transfer-paced regimes: `event ≈ true kernel + ~5–7 µs` launch gap → usable.
- In kernel-saturated regimes: **inflated up to 3.5×** → **not quotable**.
- Symptom of saturation: the derived `PCIe + overhead = wall/N − kernel_ms` **goes
  negative** (kernels overlap, so wall/frame < kernel/frame).

Ground truth requires Nsight Systems (§8).

### 3.3 Profiler distorts wall clock

Under `nsys`, wall time per frame is ~4× the unprofiled value (API tracing overhead).
**Take per-operation GPU times from nsys; take wall times from unprofiled runs.** The
standalone probe script also pays the full first-touch fault tax inside its single
timed call (fresh process, no warm pass), so **its wall times are not throughput
numbers** either.

### 3.4 Other controls

- GPU clock ramp (210 MHz → 3.1 GHz) is real but < 0.1 % of a multi-second run; the
  invariance of `kernel_ms` across loaded/idle runs confirms it is not a factor.
- The `cf_cuda_v1` notebook cell builds a histogram **inside** its timed loop
  (~4.5 s over 100 k frames). It is a reference row, **not part of the arc**.
- `ClusterFinderMT` cannot restart after `stop()`; the CPU baseline is therefore
  always a first-pass number and carries its own ~1.8 s of allocator faults.

---

## 4. Campaign A — 3×3 pipeline arc (f64 pedestal) ★ headline

**Config**: `cluster_size=(3,3)`, `N=100 000`, `n_frames_pd=1000`, `n_sigma=5`,
`BATCH_SIZE=2000`, `n_streams=4`, `max_clusters_per_frame=3000`.
**Source**: [`python/tests/ClusterFinderCUDA_perf.ipynb`](../python/tests/ClusterFinderCUDA_perf.ipynb),
warm pass (steady state, faults plateaued).

| step | variant | wall [s] | FPS | µs/frame | kernel [µs/fr] | host ovhd [µs/fr] | **vs CPU** | step gain | minor faults |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|
| baseline | CPU MT (48 threads) | 21.00 | 4 761 | 210.0 | — | — | 1.00× | — | 2.63 M |
| **opt1** | 1 stream, per-frame | 6.68 | 14 968 | 66.8 | 23 | 44 | **3.14×** | 3.14× | 1 |
| **opt2** | 4 streams + batching | 4.32 | 23 134 | 43.2 | 22 | 21 | **4.86×** | 1.55× | 64 931 |
| **opt3** | pipeline rework, no pin | 3.76 | 26 588 | 37.6 | 24 | 14 | **5.58×** | 1.15× | 59 814 |
| **opt4** | + pinned input (DMA) | 2.72 | 36 810 | 27.2 | 24 | 3 | **7.73×** | 1.38× | 1 027 |
| **opt5** | + CUDA Graph | 2.53 | 39 472 | 25.3 | 25 * | — | **8.29×** | 1.07× | 1 032 |

\* graph reports kernel+PCIe+overhead combined; the kernel is not timed separately.

Reference row (not in the arc): current finder, per-frame, histogram inside the timed
loop — 11.35 s / 8 813 FPS / 1.85×.

**Status: quotable.** All CUDA rows at steady state; CPU baseline is first-pass by
necessity (see §3.4) — if a fault-corrected CPU (≈19.2 s) is preferred, all speedups
shrink by ~9 % (e.g. opt5 8.29× → 7.57×). Be consistent and state the choice.

### The story in one row — host overhead

```
opt1 → opt2 → opt3 → opt4        host overhead per frame
 44  →  21  →  14  →   3 µs      (kernel fixed at ~23 µs)
```

opt4's 3 µs means H2D/kernel/D2H are essentially fully overlapped; opt5 then trims
residual launch overhead. **After opt4 the 3×3 pipeline is host/PCIe-bound, not
kernel-bound** — which is exactly why opt6 does nothing here (§5).

---

## 5. Campaign B — opt6 at 3×3 (100 % f32)

Same config as Campaign A; rebuilt with `DEVICE_PED_TYPE = float`.
**Source**: same notebook, warm pass.

| step | wall f64 → f32 [s] | FPS f64 → f32 | kernel f64 → f32 [µs] | Δ wall |
|---|--:|--:|--:|--:|
| CPU baseline | 21.00 → 21.45 | 4 761 → 4 662 | — | (run-to-run) |
| v1 per-frame (serialized) | 11.35 → 10.57 | 8 813 → 9 465 | **24 → 13** | **−7 %** ✓ |
| opt3 (no pin) | 3.76 → 3.88 | 26 588 → 25 798 | **24 → 14** | +3 % (noise) |
| opt4 (pinned) | 2.72 → 2.83 | 36 810 → 35 372 | **24 → 16** | +4 % (noise) |
| opt5 (graph) | 2.53 → 2.53 | 39 472 → 39 503 | — | **0 %** |
| opt1 / opt2 | 6.68 → 6.73 / 4.32 → 4.88 | — | 23 / 22 unchanged | n/a — f64-pinned class |

**Result: at 3×3, opt6 halves the kernel and buys nothing end-to-end.** The kernel
(13–16 µs) sits *below* the ~25–27 µs/frame transfer+host floor, so it is entirely
hidden by stream overlap. The control is the serialized `v1` path, where the kernel
*cannot* hide: there the saving does appear (−7 % wall ≈ the 8–10 µs/frame kernel gain).

The f32 opt2 row (4.88 s) still carried 822 k faults and is **not quotable**; it is
also irrelevant, being the f64-pedestal class.

**Status: quotable** (opt3/opt4/opt5/v1 rows).

---

## 6. Campaign C — opt6 at 9×9 (where the kernel *is* the bottleneck) ★

**Config change**: `cluster_size=(9,9)`, `N=20 000`, `n_streams=8`,
`max_clusters_per_frame=1500`, everything else unchanged.
Cap 1500 chosen so the CUDA finders record **all** clusters the CPU finds.
**Source**: same notebook, warm runs (faults ≤ 102).

| build | path | wall [s] | FPS | µs/frame | kernel [µs/fr] | derived ovhd | vs CPU | clusters/frame |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| **f64 ped** | opt4 batched+pin | 1.540 | 12 988 | 77.0 | 171 † | **−94** † | 6.01× | 1422.13 |
| **f64 ped** | opt5 graph | 1.482 | 13 491 | 74.1 | 74 * | — | 6.24× | 1422.13 |
| **f32 ped** | opt4 batched+pin | 1.423 | 14 057 | 71.2 | 33 | +38 | 6.61× | 1422.33 |
| **f32 ped** | opt5 graph | 1.372 | 14 580 | 68.6 | 69 * | — | 6.86× | 1422.33 |

† **Not quotable** — event-timer inflation under 8-stream saturation; the negative
overhead is the tell-tale. True kernel time is 43 µs (§8). \* combined metric.

CPU baselines (derived from the printed speedup ratios): f64 run ≈ 9.25 s (2 162 FPS),
f32 run ≈ 9.41 s (2 126 FPS).

**Result: at 9×9, opt6 is worth ~8 % end-to-end** (1.540 → 1.423 s batched;
1.482 → 1.372 s graph) **and flips the regime**: the f64 build is kernel-bound
(negative derived overhead), the f32 build is transfer-bound (+38 µs).

**Status: wall/FPS/counts quotable; kernel column must come from §8.**

---

## 7. The rule that unifies Campaigns B and C

Per-frame GPU operation profile at 9×9 (nsys, §8):

```
f64:  kernel 43 µs | D2H 19 µs | H2D 13 µs     → kernel is the tallest bar
f32:  kernel 26 µs | D2H 20 µs | H2D 13 µs     → near-balanced
3×3:  kernel 13–24 µs, below the ~27 µs host/PCIe floor → always hidden
```

> **f32 buys exactly the distance between the kernel bar and the next-tallest bar.**
> Nothing at 3×3 (kernel already hidden); ~8 % at 9×9/f64 (kernel on the critical
> path); more for larger windows, faster links, or fewer streams.

---

## 8. Nsight Systems ground truth (9×9, cap 1500)

**Script**: [`python/tests/nsys_kernel_probe.py`](../python/tests/nsys_kernel_probe.py)
(also in the session scratchpad). Trains 1 000 pedestal frames, runs one batched pass
over 2 000 frames, prints wall + event `kernel_ms`.

```bash
nsys profile --trace=cuda --sample=none --cpuctxsw=none -o probe_s1 \
     python nsys_kernel_probe.py 1 2000      # 1 stream  → kernels serialized
nsys profile --trace=cuda --sample=none --cpuctxsw=none -o probe_s8 \
     python nsys_kernel_probe.py 8 2000      # 8 streams → deck config
nsys stats --report cuda_gpu_kern_sum --report cuda_gpu_mem_time_sum probe_s1.nsys-rep
```

### Kernel — `aare::device::find_clusters_in_single_frame<Cluster<int,9,9,uint16_t>>`

| build | streams | instances | **avg** | median | min | max | σ |
|---|--:|--:|--:|--:|--:|--:|--:|
| f64 ped | 1 | 2 000 | **43 011 ns** | 42 945 | 40 929 | 45 632 | 580 |
| f64 ped | 8 | 2 000 | 46 714 ns | 46 880 | 41 632 | 61 441 | 1 988 |
| f32 ped | 1 | 2 000 | **25 602 ns** | 25 569 | 24 737 | 27 073 | 411 |
| f32 ped | 8 | 2 000 | 25 840 ns | 25 728 | 24 864 | 30 624 | 578 |

> ### **opt6 kernel result: 43.0 µs → 25.6 µs = −40 %** (9×9, exclusive time)

### Memory operations (1-stream, uncontended)

| op | f64 | f32 | payload | effective BW (decimal) |
|---|--:|--:|---|--:|
| D2H (clusters) | 19 424 ns | 19 845 ns | 1500 × 328 B = 492 000 B (480.5 KiB) | **25.3 GB/s** (23.6 GiB/s) |
| H2D (frame) | 13 210 ns | 13 451 ns | 400×400×2 B = 320 000 B (312.5 KiB) | **24.2 GB/s** (22.6 GiB/s) |
| memset | 359 ns | 365 ns | — | — |

The H2D payload is **one frame**; the D2H payload is one frame's cluster buffer
(`max_clusters_per_frame` × `sizeof(Cluster<int,9,9,uint16_t>)`, transferred at
fixed size regardless of how many clusters were actually found).

PCIe 4.0 ×16 theoretical is 31.5 GB/s (16 GT/s × 16 lanes × 128b/130b), so H2D
reaches **77% of theoretical** — the signature of a real DMA path. Pageable
transfers, which the driver stages through a hidden pinned buffer, run ~15 GB/s.

Transfers are identical between builds and at full DMA speed — confirming pinning
(opt4) is doing its job and that only the kernel changed.

### Cross-validation of the event timer

| build | streams | nsys avg | event `kernel_ms` | offset |
|---|--:|--:|--:|--:|
| f64 | 1 | 43.0 µs | 0.050 ms | +7.0 µs (launch gap) |
| f64 | 8 | 46.7 µs | 0.149 ms | **+102 µs (queue-wait)** |
| f32 | 1 | 25.6 µs | 0.033 ms | +7.4 µs |
| f32 | 8 | 25.8 µs | 0.033 ms | +7.2 µs |

Independently reproduced unprofiled by the user (f32 build, `N=20000`): event
`kernel_ms` = **0.030–0.032** warm (= 25.6 µs + ~5–6 µs gap); 0.047 and 0.184 on
cold-clock first invocations.

Note the 8-stream instance time stretches only +9 % (f64) / +1 % (f32): **one 9×9
kernel nearly fills the GPU, so streams queue rather than co-execute.** Multi-streaming
at 9×9 buys transfer overlap, not kernel concurrency.

**Reports retained** (openable in `nsys-ui` for timeline figures):
`probe_s1.nsys-rep`, `probe_s8.nsys-rep` (f64), `probe_f32_s1.nsys-rep`,
`probe_f32_s8.nsys-rep` (f32).

---

## 9. Supporting study — the page-fault artifact

### Synthetic isolation

Allocating ~8 GB in ClusterVector-sized chunks (90 000 × 93 kB), touching every page,
freeing, repeating in one process:

| run | wall | minor faults |
|---|--:|--:|
| 1 (cold heap) | 2.05 s | 2 046 594 |
| 2 (warm heap) | 0.07 s | 2 232 |
| 3 (warm heap) | 0.07 s | 2 016 |

**30× faster, 1000× fewer faults**, same allocations — glibc retains the arenas.

### In situ (opt2 cell, three consecutive executions)

| run | wall [s] | FPS | minor faults |
|---|--:|--:|--:|
| 1 | 6.110 | 16 366 | 2 625 948 |
| 2 | 4.872 | 20 526 | 729 866 |
| 3 | 4.452 | 22 459 | 185 885 |

Correlation Δwall vs Δfaults:

| interval | Δwall | Δfaults | implied cost |
|---|--:|--:|--:|
| 1 → 2 | 1.238 s | 1 895 667 | **0.65 µs/fault** |
| 2 → 3 | 0.420 s | 543 981 | **0.77 µs/fault** |
| 1 → 3 | 1.658 s | 2 439 648 | **0.68 µs/fault** |

Reconstruction of run 1 from run 3:
`4.452 s + 2 439 648 × 0.68 µs = 6.111 s` vs **measured 6.110 s** (1 ms error over 6 s).

Kernel time was constant (0.022 ms) across all three — the GPU is not involved.
**Conclusion: the entire first-run penalty is OS page population of the host result
heap.** Persistence mode / clock ramp are not responsible.

---

## 10. Correctness (held constant across the whole arc)

### 3×3, N = 100 000

| finder | clusters | /frame | diff vs CPU |
|---|--:|--:|--:|
| CPU MT | 233 085 343 | 2330.85 | — |
| opt1 | 233 094 770 | 2330.95 | 0.0040 % |
| opt2 | 233 093 553 | 2330.94 | 0.0035 % |
| opt3 / opt4 / opt5 (f64) | 233 093 484 | 2330.93 | 0.0035 % |
| opt3 / opt4 / opt5 (**f32**) | 233 093 554 – 233 094 465 | 2330.94 | 0.0039 % |

The residual ~0.004 % is the known per-frame vs per-pixel pedestal-update difference
(analysed in `ClusterFinderFrozen_vs_CUDA.ipynb`), **not** a precision effect.

**f64 vs f32 builds differ by ~70 clusters out of 233 M (3 × 10⁻⁷).**

### 9×9, N = 20 000

| build | clusters | /frame |
|---|--:|--:|
| f64 | 28 442 582 | 1422.13 |
| f32 | 28 446 667 | 1422.33 |

### Two fixes that made this possible

**(a) B1 — per-pixel offset accumulation** (`docs/pedestal_precision_f32_cancellation.md`).
Before: naive f32 pedestal produced **+28.06 % clusters and an unphysical high-energy
tail** — catastrophic cancellation in `var = sum2/n − mean²` (both terms ≈ 2.17 × 10⁷,
variance ≈ 2025) drove quiet pixels to `rms=0`, so they fired every frame. Fix: freeze
a per-pixel baseline `X0 ≈ round(mean)` at t=0 and accumulate centered `Y = X − X0`.
After: full-f32 matches f64 to **3 × 10⁻⁷**. **opt6 is only shippable because of B1.**

**(b) Test3 local-max gate backported** into
[`clusterfinder_kernel_opt2.cuh`](../include/aare/clusterfinder_kernel_opt2.cuh)
so opt1/opt2 stop over-counting extended charge-shared events:

| | before | after |
|---|--:|--:|
| opt1 | 233 940 268 (2339.40/fr) | 233 094 770 (2330.95/fr) |
| opt2 | 233 931 015 (2339.31/fr) | 233 093 553 (2330.94/fr) |

−0.36 %, now matching CPU. Kernel time unchanged (the gate is an early `return`).

---

## 11. Numbers that must NOT be used

| number | why |
|---|---|
| 9×9 **cap = 1000** runs (f64: 1.277 s / 15 657 FPS / 7.37×; f32: 1.087 s / 18 395 FPS / 8.53×) | cluster cap saturated → **exactly** 1000.00/frame, ~30 % of clusters truncated; CPU found ~1422/frame. Timing story survives but counts and speedups are dishonest. Superseded by cap 1500 (§6). |
| `kernel_ms` under multi-stream saturation (f64 9×9: 0.171 / 0.192; 8-stream first-launch 0.184) | event-timer queue-wait inflation up to 3.5× (§3.2). Use nsys §8. |
| Negative "PCIe + overhead" (−0.094, −0.128, −0.007) | arithmetic artifact of `wall/N − inflated kernel`. Useful only as a *kernel-bound indicator*, never as a transfer cost. |
| Any wall time from `nsys_kernel_probe.py` (5.3–6.6 s for 20 k frames) | fresh process → full first-touch fault tax inside the timed call; single mega-batch; profiler overhead when traced. |
| Cold-pass notebook numbers (opt2 6.18 s, opt3 5.29 s, batched 4.45 s, graph 4.02 s) | 2.1–2.6 M unresolved page faults each (§9). |
| `cf_cuda_v1` cell as an arc data point | builds a histogram inside the timed loop (~4.5 s / 100 k frames). Valid only as the *serialized-path* control in §5. |

---

## 12. Reproduction index

| artifact | path | role |
|---|---|---|
| main benchmark notebook | `python/tests/ClusterFinderCUDA_perf.ipynb` | all wall/FPS/count numbers; 7 instrumented timed cells |
| nsys probe | `python/tests/nsys_kernel_probe.py` | exclusive kernel + memcpy times |
| correctness notebook | `python/tests/ClusterFinderFrozen_vs_CUDA.ipynb` | CPU↔CUDA agreement analysis |
| precision study | `docs/pedestal_precision_f32_cancellation.md` | B1 derivation |
| opt1/opt2 class | `include/aare/ClusterFinderCUDAOpt2.hpp` | pre-refactor pipeline snapshot |
| opt1/opt2 kernel | `include/aare/clusterfinder_kernel_opt2.cuh` | `88e0e8d` kernel + backported Test3 gate |
| opt3/opt4/opt6 | `include/aare/ClusterFinderCUDA.hpp`, `include/aare/clusterfinder_kernel.cuh` | current pipeline; precision knob at lines 16–17 |
| opt5 | `include/aare/ClusterFinderCUDA_graph.hpp` | graph-based finder |
| deck | `docs/ClusterFinderCUDA_optimizations.pptx` | target |

### Protocol to reproduce a clean number

1. Idle machine. Optionally `sudo nvidia-smi -pm 1` (kills ~1–3 s of per-process
   driver init; does **not** affect the fault artifact).
2. Restart the Jupyter kernel; run the notebook top to bottom (cold pass).
3. **Re-run each timed CUDA cell** until its `minor faults` line plateaus
   (< ~200 k; usually 2–3 executions). Quote that run.
4. The CPU baseline cannot be re-run (`ClusterFinderMT.stop()` is terminal) —
   re-run the *Build finders* + pedestal cells first if a warm CPU number is needed.
5. For kernel times: use `nsys` at `n_streams=1`, never the event timer under load.

---

## 13. Slide-ready takeaways

1. **The pipeline arc (opt1 → opt5) is monotonic**: 3.14× → 4.86× → 5.58× → 7.73× →
   **8.29×** over a 48-thread CPU baseline, at identical correctness (0.004 %).
2. **The optimization story is host overhead collapsing**: 44 → 21 → 14 → **3 µs/frame**
   against a fixed ~23 µs kernel. By opt4 the GPU is fed almost perfectly.
3. **opt6 (f32 pedestal) is a kernel win, not always a throughput win**: kernel
   **43.0 → 25.6 µs (−40 %, nsys-verified)**, but end-to-end **0 % at 3×3** and
   **−8 % wall at 9×9**. The bottleneck is workload-dependent.
4. **The unifying rule**: f32 buys the distance between the kernel bar and the
   next-tallest bar in the per-frame GPU profile.
5. **opt6 is only correct because of the B1 variance rewrite** — the naive f32 pedestal
   gave +28 % clusters and an unphysical tail; the centered accumulation restores
   agreement with f64 to 3 × 10⁻⁷.
6. **Measurement discipline was necessary to get here**: soft page faults (0.7 µs each,
   up to 4 s per run) and CUDA-event queue-wait inflation (up to 3.5×) both had to be
   identified and controlled before any number was trustworthy.
7. **Where the next win is**: after opt4 the 3×3 pipeline is PCIe/host-bound. Further
   kernel work has no payoff at that window size; the next target is transfer volume
   (on-GPU reduction, keeping results on device) rather than kernel speed.
