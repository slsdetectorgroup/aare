# ClusterFinderCUDA — Benchmark Results

Verified performance numbers for the CUDA cluster-finder optimization ladder, told in
the order the bottleneck actually moves: **feed the GPU → get the results back → make
the GPU faster.** Every number is traced to a file in
[`python/tests/perf/results/`](../python/tests/perf/results/), and each is tagged
**quotable** or **not quotable** with the reason.

> **Reading order.** §2 defines the ladder and §4 defines the measuring stick. After
> that the three acts (§5–§7, §8–§10, §11) are self-contained. §16 is the slide-ready
> summary.

---

## 0. Provenance — where every number comes from

The measurement harness is [`python/tests/perf/`](../python/tests/perf/) (see its
`README.md`); results live in `perf/results/<date>_<build>/`, each carrying an
`env.json` (build, git rev, driver, GPU) and a `manifest.csv` mapping artifact → config
→ build → citing section.

| tag | directory | contents |
|---|---|---|
| **`[f64]`** | [`perf/results/2026-08-18_f64/`](../python/tests/perf/results/2026-08-18_f64/) | `ladder_3x3.csv`, `ladder_9x9.csv`, `probes.csv` — `DEVICE_PED_TYPE=double`, **cap 1500 at 9×9**. **3×3 for Acts I and II.** |
| **`[f32]`** | [`perf/results/2026-08-18_f32/`](../python/tests/perf/results/2026-08-18_f32/) | same, `DEVICE_PED_TYPE=float`. **3×3 for Act III.** |
| **`[f64]` 9×9** | [`perf/results/2026-08-20_f64_cap1700/`](../python/tests/perf/results/2026-08-20_f64_cap1700/) | `ladder_9x9.csv` at the **lossless cap 1700** — the re-take every 9×9 end-to-end number in this document comes from. |
| **`[f32]` 9×9** | [`perf/results/2026-08-20_f32_cap1700/`](../python/tests/perf/results/2026-08-20_f32_cap1700/) | same, `float`. |
| **cap A/B** | [`perf/results/2026-08-20_{f64,f32}_capAB/`](../python/tests/perf/results/) | `probes.csv` with **both caps probed in one session**, s1 and s4 — the source of every 9×9 engine number in §4, §11 and §14. |

All five directories are git rev `7177f00` on `bench/opt2-pipeline`, same driver and GPU;
`env.json` records each. Reproduce either arm with
`./run_campaign.sh f64` (or `f32`); the arm is selected by one line in
`include/aare/clusterfinder_kernel.cuh` and nothing else differs. The `step` column in
the CSVs carries the harness's internal labels — §15 maps them to the step names used
here.

**Campaign parameters**, fixed across every step so the ladder measures the code and not
the configuration:

| | 3×3 | 9×9 |
|---|--:|--:|
| `N` | 100 000 | 20 000 |
| `max_clusters_per_frame` | 3 000 | **1 700** |
| `n_streams` | 4 | 4 |
| `BATCH_SIZE` | 2 000 | 2 000 |
| pedestal frames / `n_sigma` | 1 000 / 5 | 1 000 / 5 |
| reps | 5 | 5 |
| nsys probe frames | 20 000 | 20 000 |

The 9×9 cap was **1 500 in the 2026-08-18 campaign and 1 700 from 2026-08-20 on**: 1 500
sat below the observed per-frame maximum of 1 633 and silently truncated 0.0095 % of
clusters (§3.5). Every 9×9 number in this document is at 1 700 unless the text says
otherwise; §12.1–§12.2 keep their cap-1500 figures deliberately, because the page-fault
diagnosis was made at that cap and the slot size is stated alongside. The cap is not a
free parameter at 9×9 — it sets the D2H bar directly (§4.2).

9×9 is held at N = 20 000 because its result heap is ~5× larger per frame
(1422 × 328 B = 466 kB vs 2330 × 40 B = 93 kB); 100 k would need 46.6 GB to retain
against 98 GB free with no swap. Probes are 20 000 frames because shorter ones do not
let the GPU clocks ramp (210 MHz idle → 3.1 GHz) and under-report the device 7–10 %.

### Two conventions used throughout

**`cold` = rep 0 in a fresh process; `warm` = best of the remaining reps** — not the
last rep. `collect()` does not converge, it oscillates between allocator states (9×9
opt4: 85.8 / 73.7 / 86.6 µs with faults 520 k / 127 k / 519 k), so "last rep" would let
the choice of rep do the work. Each step runs in **its own process**, because the heap
is process-wide: `opt3` reports 2 faults after opt1/opt2 have run and 92 251 alone.

**At `n_streams=4` the probe's kernel column is engine occupancy** — the union of kernel
intervals per frame, not per-kernel duration. f64 9×9 reads 32.66 µs at `s4` while each
kernel is ~43.2 µs (`overlap = 1.32`). Use **`s1` for exclusive kernel times** (the
Act III claim) and **`s4` for the peak**, which is the configuration the ladder runs —
subject to §4's rule that the peak is the lower of that estimate and the best rate
actually sustained.

Companion documents:
- `docs/pedestal_precision_f32_cancellation.md` — why the naive f32 pedestal failed and how B1 fixes it
- `docs/cf_cuda_performance.pptx` — the deck these numbers feed, built by `docs/deck/build_performance_deck.py`

---

## 1. Environment

| item | value |
|---|---|
| GPU | NVIDIA GeForce RTX 4090 (Ada, sm_89), 24 GB, driver 595.71.05 |
| GPU clocks | idle 210 MHz → boost 3120 MHz; **persistence mode disabled** |
| FP64 rate | 1/64 of FP32 on this part (relevant to Act III) |
| CPU | AMD Ryzen 9 7950X, 16 cores / 32 threads |
| RAM | 125 GiB, **no swap** |
| CUDA | 12.4 (nvcc V12.4.131) |
| Profiler | Nsight Systems 2024.5.1 (`/opt/nvidia/nsight-systems/2024.5.1`) |
| Host | `pc-moench-04` |
| Branch | `bench/opt2-pipeline` @ `7177f00` (off `feature/cuda_clusterfinder`) |

**Dataset** — MOENCH, MAX IV beamtime, Cu fluorescence:

```
/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/
    Cu_factor_10_data_master_0.json        (100 000 frames, 400×400 uint16)
    Cu_factor_10_pedestal_master_0.json    (1 000 frames used for pedestal training)
```

Frame size 400×400×2 B = 320 000 B (312.5 KiB). All finders trained on the
**same 1 000 pedestal frames**; `n_sigma = 5` throughout. Data pre-loaded into RAM with
`read_n()` so file I/O is outside every timing loop.

---

## 2. The ladder — three acts

The ladder is ordered by **which bar is tallest**. Each act removes the current binding
constraint and thereby *creates* the motivation for the next one.

| act | the bar in the way | steps | build |
|---|---|---|---|
| **I — feeding the GPU** | host submission and transfer overhead | opt1 · opt2 · opt3 · opt4 | `[f64]` |
| **II — getting results back** | the host result path | opt5 · opt6 | `[f64]` |
| **III — the kernel itself** | the kernel, *now that nothing else is in the way* | opt7 | **`[f32]`** |

The build axis falls on the act boundary: Acts I and II are entirely f64, and the flip
to f32 **is** Act III.

| step | what changed | how it is measured |
|---|---|---|
| **baseline** | `ClusterFinderMT` at its **best** thread count — 24 at 3×3, 32 at 9×9 (§4.1) | `ClusterFinderMT(..., n_threads=N)` + `ClusterCollector` |
| **opt1** | first CUDA port: 1 stream, one launch per frame, no batching | `ClusterFinderCUDAOpt2(..., n_streams=1)` + `find_clusters()` per frame |
| **opt2** | multi-stream scaffolding + host-side batching (bulk memcpy) | `ClusterFinderCUDAOpt2(..., n_streams=4)` + `find_clusters_batched()`, batch 2000 |
| **opt3** | pipeline rework: remove per-round sync barriers, fixed-size D2H | `ClusterFinderCUDA(..., n_streams=4)` + `find_clusters_batched()`, **no pinning** |
| **opt4** | DMA-speed transfers via pinned host input | opt3 + `register_input_buffer(data)` |
| *route A* | *CUDA Graphs — pre-recorded H2D→kernel→D2H per stream* | **rejected**, §7 |
| **opt5** | host↔GPU overlap: the batch is chunked and chunk i+1 submitted before chunk i is collected | §8; now internal to `find_clusters_batched()` |
| *route B′* | *one allocation per chunk (`collect_packed`)* | **rejected**, §10 |
| *route B″* | *parallel materialization over a thread pool* | **rejected**, §10 |
| **opt6** | zero-copy collection: results are read in place from the pinned D2H buffer instead of being copied into per-frame `ClusterVector`s | `collect_view()` / `find_cluster_views_batched_iter()` — §9 |
| **opt7** | **the kernel**: f32 device pedestal + B1 variance rewrite | rebuild with `DEVICE_PED_TYPE = float` — §11 |

### 2.1 The fork after opt4, and three rejected routes

Both routes out of opt4 need pinned input, so the split comes after it, not before.

**Route A — CUDA Graphs** (§7) attacks per-frame launch API cost. Against Act I's
bottleneck this was the right target; by the end of Act II launch overhead no longer
binds, and the graph finder never received the chunked pipeline of opt5.

**Routes B′ and B″** (§10) both attack the 467 kB/frame result copy without removing it
— one allocation per chunk, and parallel materialization. Both lose to the allocator.

All three are documented rather than deleted, because the reasoning that motivated them
was sound against the bottleneck of the time and the bottleneck moved. That is the thesis
of this document, and the failures are better evidence for it than the successes.

**Event removal is not a rung.** CUDA-event kernel timing costs 10–15 % of end-to-end
throughput (§3.2) to produce a number that is unusable under multi-stream load. It is
switched off on *all three* finders for the whole campaign — a comparability requirement,
not an optimization.

### Code behind each step

| step | primary source |
|---|---|
| opt1, opt2 | [`include/aare/ClusterFinderCUDAOpt2.hpp`](../include/aare/ClusterFinderCUDAOpt2.hpp), [`include/aare/clusterfinder_kernel_opt2.cuh`](../include/aare/clusterfinder_kernel_opt2.cuh) — snapshot of commit `88e0e8d` (pre-refactor pipeline), namespace `aare::device_opt2` |
| opt3 – opt7 | [`include/aare/ClusterFinderCUDA.hpp`](../include/aare/ClusterFinderCUDA.hpp), [`include/aare/clusterfinder_kernel.cuh`](../include/aare/clusterfinder_kernel.cuh) |
| route A | [`include/aare/ClusterFinderCUDA_graph.hpp`](../include/aare/ClusterFinderCUDA_graph.hpp) |
| bindings | [`python/src/bind_ClusterFinderCUDAOpt2.hpp`](../python/src/bind_ClusterFinderCUDAOpt2.hpp), [`python/src/cuda_bindings.cu`](../python/src/cuda_bindings.cu) |
| factories | [`python/aare/ClusterFinder.py`](../python/aare/ClusterFinder.py) |

Relevant history: `3ed773e` (multi-stream+batched) → `ac96d1f` (mixed precision) →
`88e0e8d` (**opt1/opt2 snapshot**) → `6a12e3d` (pipeline refactor = opt3) →
`4c66802` (FP32 pedestal, introduced the tail) → `5922c73` (async API) →
`1bf317f` (local-max fix) → `a42d71c` (graphs = route A).

### Precision configuration

Two type aliases in [`clusterfinder_kernel.cuh:16-17`](../include/aare/clusterfinder_kernel.cuh#L16-L17):

```cpp
using COMPUTE_TYPE    = float;   // stencil arithmetic — float in ALL builds below
using DEVICE_PED_TYPE = double;  // device pedestal — the opt7 knob (double → float)
```

- **"f64 build"** = `COMPUTE_TYPE=float`, `DEVICE_PED_TYPE=double` (mixed precision). **Acts I–II.**
- **"f32 build"** (opt7) = `COMPUTE_TYPE=float`, `DEVICE_PED_TYPE=float` (100 % f32). **Act III.**

> ⚠️ `ClusterFinderCUDAOpt2` is templated on `PEDESTAL_TYPE`, which its binding pins
> to `double` ([`bind_ClusterFinderCUDAOpt2.hpp:14`](../python/src/bind_ClusterFinderCUDAOpt2.hpp#L14)).
> **opt1 and opt2 are unaffected by the opt7 flip** — their numbers are identical in both
> arms by construction, which makes them a useful cross-arm control. Only opt3–opt6
> respond.

---

## 3. Methodology — what is measurable and what is not

Three measurement artifacts were identified and controlled. **All three matter for how
numbers may be quoted on a slide.**

### 3.1 First-touch page faults — the dominant artifact

This is the single largest source of bogus numbers in this campaign, so it is worth
stating the mechanism precisely.

**What a minor fault is.** A page exists in the process's virtual address space but has
no physical frame behind it yet. On first touch the kernel finds a free frame, **zeroes
it** (mandatory, for security), and maps it. No disk I/O — that would be a *major*
fault, and `ru_majflt` stays at 0 throughout this work. At 4 kB/page, 1 GB of
freshly-touched memory costs **262 144 minor faults**. None of this is CUDA-specific;
it is how every anonymous allocation on Linux behaves.

#### Two sources, both counted as `ru_minflt`

`getrusage` cannot distinguish them, but they behave completely differently:

| | **(a) result heap** | **(b) pinned D2H slots** |
|---|---|---|
| allocator | `malloc` → `mmap`; one `ClusterVector` per frame, in `collect()` **and in the CPU finder** | `cudaMallocHost` in `submit_batch` |
| trigger | first write after allocation | slot creation (or re-creation) |
| measured cost | **0.7 µs/page** (§12) | **1.0 µs/page** (§12.1) — the same fault, plus pinning and DMA mapping |
| recurs? | **yes** — every alloc/free cycle. `free` above glibc's mmap threshold `munmap`s, handing pages back to the OS | **no** — once per buffer, for its lifetime |
| control | reuse the heap: re-run until the counter plateaus; drop the previous results **before** entering the timed region. **Only partly effective at 9×9**, where the plateau is ~292 k/pass rather than 0 (§12.2) | `reserve_output_slots()` before starting the timer |
| eliminated by | `collect_view()` — it allocates nothing (opt6, §9) | nothing; it is a one-off startup cost, not a per-frame cost |

The CPU baseline exhibits (a) and never (b) — it pins nothing. That is the cleanest
demonstration that (a) is a property of the allocator, not of CUDA.

**What does *not* fault**, and is therefore never the explanation:

- the GPU's D2H write into the pinned slot — pinning guarantees residency before any
  DMA, which is the entire reason the buffer is pinned;
- the host *reading* that slot in `collect()` / `sums()` — those pages are resident.
  `collect()`'s faults come from its heap **destination**, not from its source;
- `register_input_buffer` — `cudaHostRegister` pins pages the `data` array already
  touched when it was read from file.

#### Instrumentation

Bracketing every timed region:

```python
import resource
def _faults():
    r = resource.getrusage(resource.RUSAGE_SELF)
    return r.ru_minflt, r.ru_majflt
```

#### Diagnostic procedure ★

When a run is slower than expected, resolve the fault count before looking anywhere else:

1. **Is it the whole discrepancy?** Multiply the count by 0.7–1.0 µs and compare to the
   gap in wall time. If it accounts for the gap, stop — there is no other bug.
2. **Which source?** Multiply the count by 4 kB and name the buffer:
   - ≈ Σ clusters × `sizeof(ClusterType)` → **(a) heap**;
   - ≈ `NUM_SLOTS × chunk × m_output_bytes_per_frame` → **(b) pinned slots**.
   The second is easy to compute exactly and in practice matches to four digits (§12.1)
   — which makes it a positive identification rather than a guess.
3. **Re-run.** (a) decays toward a plateau as the heap is reused. (b) is *identical on
   every run* if the finder is constructed inside the timed region, and *zero* if it is
   not — because the slots survive with the object.
4. **Confirm (a)** with `MALLOC_ARENA_MAX=1`, which collapsed 2.27 M faults to 138 k in
   the parallel-materialization experiment (§10).
5. **Confirm the cost model** by correlating Δwall against Δfaults over successive runs
   (§12) — it reconstructs run 1 from run 3 to 1 ms over 6 s.

**Protocol: quote the run where the fault counter has plateaued.** Cold numbers are
inflated by 25–45 %. "Plateaued" means *stable across consecutive runs*, not below any
fixed threshold: at 3×3 with `collect()` the plateau is 0, but at 9×9 it is ~292 k/pass
and stays there forever (§12.2). Judge by stability, and if the plateau is non-zero, say
so alongside the number.

### 3.2 CUDA-event `kernel_ms` inflates under multi-stream saturation

`avg_kernel_time_ms()` uses a CUDA event pair on the kernel's own stream. It measures
**elapsed time on that stream's timeline**, which includes queue-wait when other streams
are competing for SMs. Consequences:

- In transfer-paced regimes: `event ≈ true kernel + ~5–7 µs` launch gap → usable.
- In kernel-saturated regimes: **inflated up to 3.5×** → **not quotable**.
- Symptom of saturation: the derived `PCIe + overhead = wall/N − kernel_ms` **goes
  negative** (kernels overlap, so wall/frame < kernel/frame).

Ground truth requires Nsight Systems (§4).

The instrumentation is also **not free**. Warm-vs-warm at 3×3, `N = 100 000`, identical
runs with only the flag differing:

| path | events ON | events OFF | Δ |
|---|--:|--:|--:|
| `find_clusters_batched` | 27.4 µs | 24.6 µs | **−2.8 µs (−10 %)** |
| `submit`/`collect` serial | 29.9 µs | 26.0 µs † | **−3.9 µs (−13 %)** |
| `submit`/`collect` pipelined | 23.3 µs | 19.9 µs | **−3.4 µs (−15 %)** |

† fault-corrected. An independent prediction from API accounting gives **3.64 µs/frame**;
three end-to-end paths land at 2.8–3.9 µs. The two methods agree.

> **The kernel-timing instrumentation costs 10–15 % of end-to-end throughput to produce a
> number that is itself unusable under multi-stream load.** `time_kernels=False` is the
> default on all three finders and is held there for the whole campaign; kernel times
> come from nsys.

This matters for comparability, not just cost: with events on for one finder and off for
another, the instrumented one pays a per-frame tax the other does not and the step
between them absorbs it. `ClusterFinderCUDAOpt2` gained the flag for exactly this reason,
so opt1/opt2 are measured on the same terms as opt3+.

### 3.3 Profiler distorts wall clock

Under `nsys`, wall time per frame is ~4× the unprofiled value (API tracing overhead).
**Take per-operation GPU times from nsys; take wall times from unprofiled runs.** The
standalone probe script also pays the full first-touch fault tax inside its single timed
call (fresh process, no warm pass), so **its wall times are not throughput numbers**
either.

The reverse also holds and is easy to forget: **a probe roofline is a mild over-estimate
of the floor**, by 1.9–6.8 % here. The cause is not dilated durations — the `_KERNEL`
table is hardware-stamped, and the `[f32]` 9×9 4-stream probe puts the binding D2H bar at
25.24 µs against 25.14 µs sustained — 0.4 % apart. It is that `kernel_us_per_frame` is a **union of intervals measured in
a loop the profiler slows to ~69 µs/frame**: sparser submission means less overlap, so the
union per frame reads high. §4 handles this by defining peak as the lower of the estimate
and the best sustained rate.

### 3.4 Other controls

- GPU clock ramp is < 0.1 % of a multi-second run; the invariance of `kernel_ms` across
  loaded/idle runs confirms it is not a factor for the ladder. It *is* a factor for short
  probes, which is why probes are 20 000 frames (§0).
- **The GPU must be idle.** `run_ladder.py` and `run_probes.py` both abort above 5 %
  utilisation: a competing process leaves per-operation averages intact while destroying
  the duty cycle and the wall clock, so the failure is silent if you do not check.
- `ClusterFinderMT` cannot restart after `stop()`; the CPU baseline is therefore always a
  first-pass number and carries its own ~1.8 s of allocator faults. Every speedup column
  below divides by a **cold** CPU and reads ~9 % generous.
- **Never warm up by processing frames.** The kernel pushes a pedestal update per pixel
  per frame, so a finder that has seen extra frames is no longer comparable with one that
  has not. `reserve_output_slots()` pre-pays the pinned allocation without transferring
  or launching anything — verified to leave cluster counts bit-identical (§12.1).

### 3.5 The CPU baseline — the thread count was wrong ★

Every speedup in this document divides by `ClusterFinderMT`. The campaign originally ran
it with `n_threads=48`. **`pc-moench-04` is a Ryzen 9 7950X: 16 physical cores, 32
logical.** 48 threads oversubscribes it by 1.5×, so the denominator was not the CPU's
throughput — it was the CPU's throughput under contention it need not have had.

**Source**: [`cpu_threads.py`](../python/tests/perf/cpu_threads.py) →
`results/2026-08-19_cpu_threads/cpu_threads.csv`. Same 1000 pedestal frames, same caps,
same frame counts and same retain semantics as the `cpu` step in `ladder.py`; a fresh
finder per point, one pass each (`stop()` is terminal).

| threads | 3×3 FPS | 3×3 µs/f | 9×9 FPS | 9×9 µs/f |
|--:|--:|--:|--:|--:|
| 8 | 3 805 | 262.8 | 737 | 1357.0 |
| 16 | 6 594 | 151.6 | 1 237 | 808.4 |
| **24** | **6 762** | **147.9** | 1 348 | 741.6 |
| **32** | 5 942 | 168.3 | **1 503** | **665.2** |
| 48 | 5 121 | 195.3 | 1 338 | 747.3 |

The 48-thread row reproduces the campaign's baseline to within run noise and lands on
identical cluster counts (233 085 343 at 3×3, 28 438 072 at 9×9), which is what
establishes that this sweep measures the same thing the ladder did.

Three consequences:

1. **The baseline moves, so every speedup does.** 3×3 divides by **6 762** (was 4 971 /
   5 229) and 9×9 by **1 503** (was 1 292 / 1 304). The 3×3 headline drops from ×11.7 to
   **×9.1** and the 9×9 headline from ×32.4 to **×28.1**. Nothing about the GPU changed;
   the CPU was being undersold by 24 % at 3×3 and 15 % at 9×9.
2. **The optimum is per cluster size** — 24 threads at 3×3, 32 at 9×9. `ClusterCollector`'s
   drain is inside the timed region (`_drive()` does the drain before the timer stops, so
   the GPU rows' own collection is the comparable thing), and that drain scales with thread
   count while 9×9 clusters are 9× larger. At 9×9 the *loop alone* keeps getting faster all
   the way to 48 threads (800 → 2 449 FPS); it is the drain that turns the curve over.
3. **There is no per-arm CPU baseline any more.** The old `[f64]` 201.16 µs and `[f32]`
   191.26 µs were the same code measured twice — `ClusterFinderMT` never touches
   `DEVICE_PED_TYPE`. The 5 % between them was run-to-run noise being reported as a build
   difference. One baseline per cluster size now serves every bar in both arms.

Two timings are recorded per point because the campaign and
`ClusterFinderCUDA_perf.ipynb` do not measure the same region: `loop_s` is the
`find_clusters()` loop alone (what the notebook prints), `wall_s` adds `stop()` and the
collector drain (what `ladder.py` records, and what the GPU rows are comparable to). All
figures above are `wall_s`.

---

## 4. The measuring stick — which bar is tallest ★

Everything after this section is read against one table. **Source**: `probes.csv` in
`[f32]` and `[f64]`, 20 000 frames, [`gpu_span.py`](../python/tests/perf/gpu_span.py)
interval-union analysis.

| config | build | kernel | H2D | D2H | bottleneck | roofline |
|---|---|--:|--:|--:|---|--:|
| 3×3, 4 str | f64 | 15.17 | **16.17** | 7.69 | H2D (barely — 1.0 µs apart) | 16.17 µs → 61 859 FPS |
| 3×3, 4 str | f32 | 5.53 | **16.63** | 7.57 | H2D (decisively — 11.1 µs) | 16.63 µs → 60 140 FPS |
| 3×3, 1 str | f64 | **14.72** | 13.14 | 5.31 | **kernel** | 14.72 µs → 67 918 FPS |
| 3×3, 1 str | f32 | 4.32 | **13.15** | 5.27 | H2D | 13.15 µs → 76 042 FPS |
| 9×9, 4 str | f64 | **32.66** | 20.77 | 25.25 | kernel | 32.66 µs → 30 621 FPS |
| 9×9, 4 str | f32 | 23.94 | 20.54 | **25.24** | **D2H** (opt7 dropped the kernel under it) | 25.24 µs → 39 614 FPS |
| 9×9, 1 str | f64 | **39.86** | 13.20 | 21.97 | kernel | 39.86 µs → 25 086 FPS |
| 9×9, 1 str | f32 | **23.70** | 13.22 | 21.95 | kernel | 23.70 µs → 42 197 FPS |

PCIe is full-duplex, so the floor is `max(H2D, D2H, kernel)` — **never the sum**.

### Peak throughput: the definition every percentage in this document uses

> **Peak = 1 / max(H2D, kernel, D2H)**, where each term is that engine's **busy time
> per frame** — the union of its intervals divided by frame count — at the ladder's
> **4 streams**; and taken as **the lower of two estimates: the profiled engine
> occupancy above, and the best rate the unprofiled pipeline sustained.**

Three things this pins down, each of which was got wrong at some point:

1. **Union, not duration.** At 9×9 `[f64]` one kernel *lasts* 43.18 µs at 4 streams,
   but the engine is occupied only 32.66 µs per frame because kernels overlap 1.32×.
   Quoting the duration understates the machine by a third. §14 rejects the reverse
   error — quoting `s4` as if it were a kernel duration.
2. **At 4 streams, not 1.** The floor is a property of *(kernel, stream count)*, not of
   the kernel. At 9×9 `[f64]` four streams lower it from 39.86 to 32.66 µs; at `[f32]`
   the kernel is short enough that overlap reaches only 1.02× and four streams *raise*
   it, 23.70 → 23.94 — where D2H, at 25.24 µs, has already overtaken it. The `1 str` rows answer "how long is one kernel", never "how fast
   can this go".
3. **Lower of the two, because a sustained rate is an existence proof.** The probe is an
   estimate made in a loop nsys slows to ~69 µs/frame; sparser submission means less
   overlap, so its union per frame reads high. Where the pipeline sustained better, that
   is the floor.

| config | probe estimate | best sustained | **peak** | which binds |
|---|--:|--:|--:|---|
| 3×3 `[f64]` | 16.17 µs | 17.10 µs | **16.17 µs → 61 859 FPS** | probe |
| 3×3 `[f32]` | 16.63 µs | 16.31 µs | **16.31 µs → 61 312 FPS** | sustained |
| 9×9 `[f64]` | 32.66 µs | 30.01 µs | **30.01 µs → 33 323 FPS** (kernel) | sustained |
| 9×9 `[f32]` | 25.24 µs | 25.14 µs | **25.14 µs → 39 775 FPS** (**D2H**) | sustained |

#### The two columns are two different runs, made by two different tools

This is the single easiest thing to get wrong, so it is worth stating flatly with 9×9
`[f64]` as the worked case:

| | **32.66 µs/frame** | **30.01 µs/frame** |
|---|---|---|
| file | `probes.csv`, `2026-08-20_f64_capAB` | `ladder_9x9.csv`, `2026-08-20_f64_cap1700` |
| harness | `run_probes.py` → `nsys_kernel_probe.py` | `run_ladder.py` → `ladder.py`, step `opt8` |
| **profiler** | **under nsys** | **none** |
| column | `kernel_us_per_frame` = `kernel_busy_ms / n_frames` | `wall_s / n_frames` = `0.60019 / 20 000` |
| what it measures | **one engine's** union occupancy | the **whole pipeline** end to end |
| scope | 653.140 ms of merged kernel intervals | 600.19 ms of wall clock |

**30.01 does not come from nsys.** It cannot: the *same profiled run* that yields 32.66
reports its own wall clock as `window_us_per_frame = 69.54 µs/frame` — 2.3× slower than
30.01 — because API tracing inflates it. That is exactly why the two harnesses are
separate, per `run_probes.py`: *"a profiled run cannot produce a throughput number, and an
unprofiled run cannot produce a per-engine breakdown. Two tools, two questions."*

The divisor is **`n_frames` = 20 000 in both cases**. `batch = 2000` is the chunk size
*inside* each run, matched between the two harnesses so the submission pattern — and
therefore the overlap — is comparable; it is never a denominator.

**Why they conflict, and which one loses.** 32.66 claims one engine needs 32.66 µs of
occupancy per frame; 30.01 says the whole pipeline emits a frame every 30.01 µs. A
pipeline cannot outrun its busiest engine, so one of them is wrong — and it is the
profiled one. Under nsys, submission is sparser, kernels overlap each other *less*, and a
less-overlapped interval set has a *larger* union. The measured 1.32× is therefore a lower
bound on the real overlap, and the true per-frame occupancy in the unprofiled run is below
32.66. Hence: the floor is the **lower** of the two, and a sustained rate outranks an
estimate.

**"Peak" here and "floor" in the deck are the same quantity**, read in opposite units: a
floor in µs/frame, its reciprocal a peak in FPS. Both are the *lower* of probe estimate
and best sustained rate, so a slide saying "at the floor" and a table saying "100 % of
peak" are the same statement. What neither means is the raw engine max — 32.66 µs at 9×9
`[f64]` is the profiled `max(H2D, kernel, D2H)`, and the floor is 30.01.

The 3×3 `[f64]` row is what keeps this from being circular: there the pipeline stopped
5.4 % **short** of the probe estimate, so the probe binds and opt6 reads 95 %, not 100 %.
Where the sustained rate does win, the probe still corroborates it — to 1.9 %, 6.8 % and
2.5 % respectively — which is the evidence that those runs were engine-bound rather than
merely fast. Quote both.

> ### **The rule that explains the whole document: an optimization buys exactly the distance between the bar it attacks and the next-tallest bar.**
> It predicts every result below, including the three that look like failures.

### The two 3×3 rooflines, and the difference between them

| | H2D/frame | FPS | meaning |
|---|--:|--:|---|
| *uncontended*, 1 stream | 13.15 µs | 76 042 | best case: no D2H in flight. **Unreachable in this config** |
| *achieved-config*, 4 streams (probe) | 16.63 µs | 60 140 | nsys's estimate of what four streams sharing one DMA engine deliver |
| **best sustained** | **16.31 µs** | **61 312** | what the pipeline actually reached — the peak, per the definition above |

Percent-of-peak figures use the **best sustained** value. The 3.2 µs between it and the
uncontended rate is
transfer granularity — 2 000 separate 320 kB descriptors — not host code, and closing it
is a different optimization from anything in Acts I–III.

### H2D↔D2H interference, measured at both sizes

H2D slows once D2H traffic runs against it, because H2D is a DMA **read** whose request
packets travel upstream against D2H's posted writes:

| | 1 stream | 4 streams | penalty |
|---|--:|--:|--:|
| 3×3 H2D | 13.15 µs | 16.63 µs | **+26 %** |
| 9×9 H2D | 13.22 µs | 20.54 µs | **+55 %** |

Both rows are `[f32]`; the 9×9 row is at **cap 1700** — at the old 1 500 the same
comparison read 13.24 → 19.74 µs (+49 %), because a smaller D2H slot leaves more of the
link for H2D. The penalty is a function of the cap, not only of the stream count.

The 1-stream figures are identical across cluster sizes (13.15 / 13.22 µs) and builds —
the payload is the same 320 000 B frame — which proves the H2D path itself is unchanged
and the difference is contention alone.

### 4.1 Engine duty cycles — is the GPU actually saturated?

`nsys stats` reports per-operation sums, which cannot distinguish "the engine was busy"
from "the engine was idle waiting". `gpu_span.py` reads the SQLite export and computes
the **union** of each engine's activity intervals over the processing window (first
kernel start → last D2H end, which excludes the pedestal-upload H2Ds that precede any
kernel). **Source**: `probes.csv`, columns `*_duty_pct`, `*_overlap`.

| config | build | kernel duty | H2D duty | D2H duty | kernel overlap |
|---|---|--:|--:|--:|--:|
| 3×3, 4 str | f64 | 65.1 % | **69.4 %** | 33.0 % | 1.06× |
| 3×3, 4 str | f32 | 24.2 % | **72.7 %** | 33.1 % | 1.00× |
| 9×9, 4 str | f64 | **47.0 %** | 29.9 % | 36.3 % | **1.32×** |
| 9×9, 4 str | f32 | 34.2 % | 29.4 % | **36.1 %** | 1.02× |

Three readings:

1. **At 3×3 the copy engine is the saturated one** (~70 % H2D in both builds) — the GPU
   spends three quarters of its time being fed. At 9×9 no single engine exceeds 47 %
   because all three are comparable and interleave; the kernel is tallest but not
   dominant.
2. **Only the kernel row ever overlaps.** `H2D_overlap` and `D2H_overlap` are **1.000 in
   every row of `probes.csv`**, s1 and s4 alike: there is one copy engine per direction,
   so same-direction transfers queue no matter how many streams issue them. For those two
   engines `busy == sum`, and the `s4` per-frame figure is still a true mean duration —
   which is why H2D *rises* 13.20 → 20.77 µs under load (each copy genuinely slows) while
   the kernel *falls*. The concurrency four streams actually buy is H2D ∥ kernel ∥ D2H,
   plus kernel-with-kernel; never copy-with-copy in the same direction. The stream
   timelines in the deck (`fig_streams`, `fig_opt2_timeline`) draw same-direction copies
   overlapping and carry a note saying so.
3. **`overlap` quantifies cross-stream kernel concurrency.** At f32/9×9 it is 1.02× —
   one 9×9 kernel nearly fills the GPU, so extra streams buy transfer overlap, not
   kernel co-execution. On f64 it rises to **1.32×**, because the longer f64 kernels
   leave more opportunity to interleave. This is why the f64 `s4` kernel column
   (32.66 µs) is well below its `s1` per-kernel duration (39.86 µs): **it is occupancy,
   not duration.** Quote `s1` when you mean "how long is the kernel".

### 4.2 The cluster cap is a throughput knob, not just a safety bound

D2H is fixed-size at `cap × sizeof(Cluster)` regardless of how many clusters were found.
At 9×9 (`sizeof = 328 B`) the cap therefore sets the D2H bar directly. Measured D2H fill
at the campaign caps: **77 % at 3×3 (cap 3000), 84 % at 9×9 (cap 1700)** — printed by
`nsys_kernel_probe.py`. This is not hypothetical: going from the old cap of 1500 to the
lossless 1700 moved D2H 22.8 → 25.2 µs, which on `[f32]` already overtakes the 23.9 µs
kernel (§11.4). Doubling to 3000 would put D2H at ~45 µs/frame on both arms. Keep
the cap at the smallest value that never truncates.

---
---

# ACT I — feeding the GPU `[f64]`

*The kernel is untouched throughout. What shrinks is everything around it.*

## 5. Act I at 3×3 ★ headline

**Source**: `ladder_3x3.csv` in `[f64]`. N = 100 000, cap 3000, 4 streams, 5 reps, each
step in its own process. Roofline: **H2D at 16.17 µs / 61 859 FPS** (§4).

| step | what changed | cold µs/f | cold FPS | warm µs/f | warm FPS | spread | **vs CPU** | step gain | % of peak |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|
| baseline | `ClusterFinderMT`, 24 threads | 147.88 | 6 762 | 147.88 | 6 762 | — | 1.00× | — | 11 % |
| **opt1** | 1 stream, one launch per frame | 63.99 | 15 628 | 63.26 | 15 807 | 1.2 % | **2.34×** | 2.34× | 26 % |
| **opt2** | 4 streams + host batching | 41.87 | 23 883 | 40.44 | 24 726 | 5.3 % | **3.66×** | 1.56× | 40 % |
| **opt3** | pipeline rework, no pinning | 34.60 | 28 901 | 34.26 | 29 188 | 2.1 % | **4.32×** | 1.18× | 47 % |
| **opt4** | + pinned input (DMA H2D) | 26.95 | 37 110 | 25.98 | 38 486 | 3.2 % | **5.69×** | 1.32× | **62 %** |

Cluster counts agree to 4 × 10⁻⁶ across every CUDA step (233 094 3xx against 233 085 343
on the CPU; the residual is the per-frame vs per-pixel pedestal-update difference of §13,
not the pipeline).

**opt4 is the largest single step in Act I (1.32×), and §4 says why**: at 3×3 the H2D bar
is the tallest, so pinning the input attacks the bar that actually binds. The same step
is worth only 1.03× at 9×9 (§6), where H2D is the *shortest* bar. This is the first
confirmation of the rule.

**Where Act I ends: 62 % of peak.** The remaining 9.8 µs/frame is host-side, and
Act II is the argument that it is *all* the result path.

**Status: quotable.** The CPU baseline is first-pass by necessity (`stop()` is terminal),
so every speedup divides by a cold CPU and reads ~9 % generous. State the convention.

---

## 6. Act I at 9×9 ★

**Source**: `ladder_9x9.csv` in `2026-08-20_f64_cap1700/`. N = 20 000, **cap 1700**,
4 streams, 5 reps. The earlier cap of 1500 sat below the per-frame maximum of 1633 and
silently truncated 0.0095 % of clusters (§3.5); every 9×9 number here is the lossless
re-take. **The CPU row is the exception**: it is the swept optimum from
`2026-08-19_cpu_threads/` (32 threads, 665.22 µs), not the ladder's own CPU row in that
directory (688.28 µs at whatever thread count `ladder.py` defaulted to). The CPU finder
ignores `max_clusters_per_frame` entirely, so the cap difference does not affect it —
but the thread count does, by 24 % (§3.5), which is why the swept value is the one
quoted. opt1/opt2 are absent: `ClusterFinderCUDAOpt2` is registered for 3×3 only, so
the 9×9 ladder starts at opt3. Peak: **30.01 µs / 33 323 FPS** — the best sustained
rate; the probe's engine-occupancy estimate is 32.66 µs / 30 621 FPS, 8.1 % lower (§4).

| step | cold µs/f | cold FPS | warm µs/f | warm FPS | spread | **vs CPU** | step gain | % of peak |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| baseline CPU, 32 threads | 665.22 | 1 503 | 665.22 | 1 503 | — | 1.00× | — | 5 % |
| **opt3** | 93.18 | 10 732 | 82.44 | 12 129 | **14.2 %** | **8.07×** | 8.07× | 36 % |
| **opt4** | 92.52 | 10 809 | 79.83 | 12 527 | **13.7 %** | **8.33×** | 1.03× | **38 %** |

Two things are different here and both matter:

1. **opt4 is worth almost nothing (1.03×)** — exactly as §4 predicts. At 9×9, H2D is
   13.20 µs against a 39.86 µs kernel; making the shortest bar shorter changes nothing.
2. **The spread is 14 %.** At 3×3 the same steps vary 2–3 %. This is not noise, it is the
   result heap: at 9×9 each pass allocates ~9.3 GB, above glibc's mmap threshold, so it is
   `munmap`ed and re-faulted every pass and never plateaus (§12.2). **A 14 % spread means
   the number depends on which run you quote** — and it is the first symptom of the
   problem Act II solves.

**Where Act I ends: 38 % of peak.** The GPU delivers a frame every 30 µs; the system
delivers one every 80. **Three quarters of the time is host-side**, and none of the
remaining device-side work can be reached until that is fixed.

---

## 7. Route A — CUDA Graphs (rejected)

The fork after opt4. Graphs pre-record H2D→kernel→D2H per stream, replacing per-frame
launch API calls with a single graph launch. Against Act I's bottleneck this was a sound
idea; it did not survive Act II.

**Source**: `ladder_*.csv` in `[f64]`.

| config | opt4 warm | route A warm | vs opt4 | spread | against the eventual opt5 |
|---|--:|--:|--:|--:|--:|
| 3×3 | 25.98 µs | 25.16 µs | ×1.03 | 0.7 % | opt5 is 19.84 → route A is **27 % behind** |
| 9×9 | 79.83 µs | **90.32 µs** | **×0.88 (worse)** | 12.1 % | opt5 is 66.39 → route A is **36 % behind** |

Three reasons it is retired rather than pursued:

1. **It is slower at 9×9 than the step it was meant to improve on**, by 12 %.
2. **Its 3×3 gain is not established.** The graph finder never recorded CUDA events while
   the stream finder did, and the event tax (2.8 µs, §3.2) is larger than the measured gap
   (0.8 µs). On an events-OFF build the two are within run-to-run noise.
3. **It never received the chunked pipeline of opt5**, and its original advantage — lower
   per-frame launch API cost — is swamped by an overlap it does not have.

Reviving it would mean giving the graph finder opt5 and opt6 first, at which point it is
competing on a per-frame API cost of ~2 µs against a 25.24 µs floor. **Launch overhead
stops being the binding constraint one step later**, so a technique aimed at it can no
longer pay.

---
---

# ACT II — getting the results back `[f64]`

*Act I left the GPU 38 % idle at 3×3 and 60 % idle at 9×9. This act is the demonstration
that all of it is the result path, and the removal of it.*

## 8. opt5 — chunked host↔GPU overlap ★

### Two orthogonal overlap axes

These are routinely conflated; they compose rather than subsume:

| axis | what it overlaps | delivered by | evidence |
|---|---|---|---|
| **GPU-internal** | H2D ∥ kernel ∥ D2H, *across streams within one batch* | opt3 | duty cycles, §4.1 |
| **host↔GPU** | host result materialization ∥ GPU execution, *across batches* | **opt5** | this section |

`find_clusters_batched` **had** the first and not the second: it synchronized, then built
thousands of `ClusterVector`s on the host with the GPU idle. Keeping one batch in flight
while materializing the previous one hides that:

```python
tok = cf.submit_batch(data[a0:b0], first_frame=a0)
for a, b in bounds[1:]:
    nxt = cf.submit_batch(data[a:b], first_frame=a)   # GPU starts batch N+1 …
    results.extend(cf.collect(tok))                   # … while host materializes batch N
    tok = nxt
results.extend(cf.collect(tok))                       # drain
```

**Source**: `ladder_*.csv` in `[f64]`.

| config | opt4 warm | **opt5** warm | step gain | spread | % of peak |
|---|--:|--:|--:|--:|--:|
| 3×3 | 25.98 µs / 38 486 FPS | **19.84 µs / 50 410 FPS** | **×1.31** | 1.2 % | 62 % → **81 %** |
| 9×9 | 79.83 µs / 12 527 FPS | **66.39 µs / 15 063 FPS** | **×1.20** | 22.4 % | 38 % → **45 %** |

> ### **opt5 result: ×1.31 at 3×3 and ×1.20 at 9×9 — for an API change with no CUDA work at all.**

The hidden time is `min(GPU, host)` by construction. A cross-check on the events-ON build
gave serial 29.9 → pipelined 23.3 µs at 3×3, hiding **6.6 µs**; on the events-OFF build
it is 26.0 → 19.9, hiding **6.1 µs**. The build changed, the absolute times changed, and
the overlap window did not — as it should not.

**No staging copy is needed.** `submit_batch` reads directly from `frames.data()`, so
slices of one already-pinned array can be handed to it as-is. The two-alternating-buffer
pattern in the `submit_batch` docstring costs ~6.5 µs/frame in `memcpy` and is only
required when frames arrive into a buffer that is being rewritten.

### 8.1 opt5 is now internal — the manual pattern is retired

The ping-pong above delivers the overlap but pushes token lifetime onto the caller. It is
now internal: `find_clusters_batched()` splits its batch into chunks and runs the same
submit-i+1-before-collect-i loop itself, so `collect()` callers get opt5 for free and the
two paths converge (×1.03–1.05 residual, i.e. noise).

Three constraints shape the chunk size, all in `resolve_batch_chunk()`
([`ClusterFinderCUDA.hpp:182`](../include/aare/ClusterFinderCUDA.hpp#L182)):

| constraint | rule | why |
|---|---|---|
| enough chunks to pipeline | `DEFAULT_BATCH_CHUNKS = 8` | fill/drain waste is `1/(C·max)`, per-chunk tail cost is `C·n_streams·kernel`; the product is flat near 8 |
| **multiple of `n_streams`** | round up | frames go to streams round-robin (`frame_idx % n_streams`) and the device pedestal is **per stream**. A chunk size that shifts the assignment changes which pedestal state a frame sees — **a correctness constraint, not tidiness** |
| bounded pinned footprint | `MAX_SLOT_BYTES = 128 MiB` | added later; see §12.1 for the failure it fixes |

`set_batch_chunk(n)` overrides the auto rule; `chunk_size_for(n)` exposes what it would
choose, so a caller driving `submit_batch`/`collect_view` by hand can match the pipelining
without duplicating the rounding rules. Setting `set_batch_chunk(batch_size)` disables
internal chunking, which is how the ladder reproduces opt3/opt4 on the current class.

### 8.2 What opt5 does *not* fix — the diagnosis that motivates opt6 ★

At 9×9, opt5 lands at **66.39 µs against a 30.01 µs peak**:

> **The GPU delivers a frame every 30.01 µs and the system delivers one every 66.39. The
> ~36 µs/frame gap is host-side, and overlap cannot hide it because the host term is the
> larger one.**

Two caveats on that sentence, both of which matter and neither of which changes it.

**It is inference, not arithmetic.** Reading the host term straight off the end-to-end
time assumes the pipelined loop attains `max(GPU, host)` exactly. That is the design, but
it is not measured here, and if overlap were imperfect the host term could be smaller
with the balance being un-overlapped GPU. The independent support is opt6: same loop,
same two slots, and it lands on **30.01 µs — 100 % of the floor** (§9), which is
`max(GPU, host)` attained exactly. The machinery demonstrably works when the host term is
small.

**66.39 µs is not a plateau value.** It is the best of five reps, and it still carries
**151 601 minor faults**. opt5 at 9×9 is the one row in the ladder that never reaches a
fault-free steady state — see §8.3.

The host path is `collect()`'s materialization loop
([`materialize_slot`, `ClusterFinderCUDA.hpp:137`](../include/aare/ClusterFinderCUDA.hpp#L137)):
per frame it reads the counter, `resize()`s a `ClusterVector` (one allocation) and
`memcpy`s **1422 × 328 B ≈ 467 kB** out of the pinned slot — ~934 MB per 2 000-frame
batch, single-threaded, at roughly memcpy bandwidth.

This also explains why pipelining helped no more at 9×9 (×1.20) than at 3×3 (×1.31)
despite the far larger absolute gap: pipelining hides `min(GPU, host)`, so it pays most
when the two terms are comparable. At 9×9 the host term is twice the GPU term, and hiding
the GPU underneath it recovers only the GPU's share.


### 8.3 opt5 at 9×9 never reaches a plateau — and what the host term actually is ★

Every other row in this document is quoted at steady state. This one cannot be. From
`ladder_9x9.csv` in `2026-08-20_f64_cap1700/`, with the §25 rate of **0.68 µs per
first-touch fault** applied out of sample:

| rep | µs/frame | minor faults | fault cost | fault-free |
|--:|--:|--:|--:|--:|
| 0 | 78.56 | 460 283 | 15.65 | 62.91 |
| **1** | **66.39** | **151 601** | **5.15** | **61.23** |
| 2 | 67.34 | 95 884 | 3.26 | 64.08 |
| 3 | 81.26 | 506 241 | 17.21 | 64.04 |
| 4 | 73.97 | 334 303 | 11.37 | 62.60 |

The fault count does not converge — 460 k → 152 k → 96 k → 506 k → 334 k — because each
rep meets a different allocator state for the 467 kB per-frame block. Contrast opt6 in the
same file: **2 072, 0, 0, 0, 0**. And contrast opt5 at **3×3**, which is clean (30–96 k
faults ≈ 0.2–0.65 µs/frame, 1.6 % spread). The contamination is specific to this one cell
of the matrix, and its cause is exactly the allocation opt6 deletes.

**The host term is ~62 µs, not ~40.** Two independent routes agree:

1. **Fault correction.** Subtracting the fault term collapses a **22 % raw spread into
   4.6 %**, at 61–64 µs. A rate fitted at 3×3 and applied unchanged here should not be
   able to do that unless it is describing the real mechanism.
2. **A rep that was already warm.** In the f32 arm, rep 3 happened to run with only
   **10 128 faults** (0.34 µs/frame) and measured **61.85 µs** with no correction at all —
   inside the corrected f64 band.

Annex A4's `opt5` row already contained this pair (`66.39 (152 k)` vs `61.85 (10 k)`,
verdict *not separable*); §8.3 is what it implies for the host term.

**Consequence.** Any figure drawing the 9×9 host cost at 40 µs understates it by about
half. The conclusion is unaffected — the host is the taller bar either way — but the
margin is roughly 2× the GPU floor, not 1.3×.

**Reproducing it.** Timing this row honestly needs a fresh process per rep and a
pre-touched result heap; without both, the number that comes out is an allocator state,
not a throughput.

**Fault-fairness warning.** Freeing a ~10 GB result heap hands it back to the allocator
and a subsequent loop reuses it, so on a cold heap the first loop pays the entire
first-touch tax and any printed ratio is meaningless. Both loops must be at plateau; drop
stale result bindings before timing.

---

## 9. opt6 — zero-copy collection ★★

Once the pipeline overlaps internally, the binding cost is no longer overlap but the
*transport*: one `ClusterVector` allocation and one copy per frame.

| path | allocations | copy | ownership | status |
|---|---|---|---|---|
| `collect()` | one per frame | full | owned | **kept** — the default |
| `collect_packed()` | one per chunk | full | owned | **rejected**, §10 |
| **`collect_view()`** | **none** | **none** | borrowed until released | **kept — opt6** |

`collect_view()` returns a `BatchView` whose `frame_data(i)` / `frame_xy(i)` are strided
numpy views directly over the pinned D2H slot. **It withholds ownership past the chunk,
not access** — every cluster's payload and coordinates are readable.
`find_cluster_views_batched_iter()` in
[`python/aare/ClusterFinder.py`](../python/aare/ClusterFinder.py) drives it as an iterator.

Two defects in the *slot* logic had to be fixed first, both found by the fault accounting
of §12.1 and both adding cost inside the timed region:

| defect | fix | effect |
|---|---|---|
| `resolve_batch_chunk` was `n_frames/8` with **no upper bound**, so one big call pinned 2.46 GB | `MAX_SLOT_BYTES = 128 MiB` caps the auto chunk **by bytes** | one large call now behaves like a loop over slices |
| the first `submit_batch` page-locks both slots **inside the caller's timer** | `reserve_output_slots(n)` — allocates only; no transfer, no launch, **no pedestal advance** | moves a one-off ~66–480 ms out of the measurement |

### Result — `[f64]`, warm, 5 reps

| config | peak (§4) | opt5 `collect()` | **opt6 `collect_view()`** | step gain | % of peak |
|---|--:|--:|--:|--:|--:|
| **3×3** cap 3000 | 16.17 µs / 61 859 FPS | 19.84 µs / 50 410 FPS | **17.10 µs / 58 495 FPS** | ×1.16 | 81 % → **95 %** |
| **9×9** cap 1700 | 30.01 µs / 33 323 FPS | 66.39 µs / 15 063 FPS | **30.01 µs / 33 323 FPS** | **×2.21** | 45 % → **100 %** |

> ### **opt6 result: the host leaves the critical path in both regimes.** 3×3 reaches 95 % of its H2D floor; 9×9 goes from 45 % to the floor itself, a **×2.21** step.

**On the 9×9 figure.** The measured 30.01 µs is 8.1 % *below* the profiled 32.66 µs floor,
against 0.4 % on `[f32]`, and the reason is specific: the f64 `s4` kernel column is an
*interval union* at `overlap = 1.32`, so the sparse submission of a profiled loop costs it
more overlap than it costs a barely-overlapping engine (`[f32]`, 1.02) — and on `[f32]` the
binding bar is D2H, a copy engine, which does not overlap with itself at all. **Read it as "at the floor"** — which is what §4's definition makes it,
since the sustained rate is the lower of the two.

### Why the margin differs so much between the two configurations

The win from `collect_view()` is `max(0, host_copy − gpu_floor)` plus the allocation it
avoids:

| | bytes copied per frame by `collect()` | memcpy at bandwidth | **host term** | GPU floor | fits underneath? |
|---|--:|--:|--:|--:|:--:|
| 3×3 | 2 330.9 × 40 B ≈ 93 kB | ~8 µs | ~9.8 µs | 16.17 µs | **yes** → small win (×1.16) |
| 9×9 | 1 422.4 × 328 B ≈ 467 kB | ~40 µs | **~62 µs** | 30.01 µs | **no** → large win (**×2.21**) |

**The two middle columns are not the same quantity, and conflating them understates the
9×9 case by about half.** "memcpy at bandwidth" is 467 kB divided by a single-threaded
copy rate — it is what the loop would cost if it were bandwidth-bound. It is not:
[`ClusterFinderCUDA.hpp:130-136`](../include/aare/ClusterFinderCUDA.hpp#L130-L136) records
that the work is *one 467 kB malloc + first-touch per frame, allocation-bound rather than
bandwidth-bound*. The host-term column is the steady-state figure derived in §8.3.

At 3×3 the copy hides under the GPU and opt5 already absorbs it, so what opt6 removes is
the per-frame allocation and the fault floor — second-order. At 9×9 the host term is
**twice** the GPU time and cannot hide at any amount of overlap. **This is the same "tallest bar" logic
as §4, applied to the host instead of the device.**

### Reproducibility is the other half of the claim

| | 3×3 spread | 9×9 spread | warm faults (3×3 / 9×9) |
|---|--:|--:|--:|
| opt5 `collect()` | 1.2 % | **12.3 %** | 30 795 / 48 608 |
| **opt6 `collect_view()`** | **0.2 %** | **0.2 %** | **0 / 0** |

> ### **opt6 is not merely the fastest — it is the only step whose throughput is *reproducible*.** Every path that allocates per frame varies 1–25 % run to run; the path that allocates nothing varies 0.2 %.

**Two things this section is *not* saying:**

- It is **not** claiming a 3×3 → 9×9 speedup. They are different workloads; each is
  compared only against its own roofline.
- The `collect_view()` column excludes downstream analysis, which is the point of the
  comparison — `collect()` copies but does not reduce, so timing it against
  `collect_view()` + `sums()` + histogram would compare different work. Measured
  separately at 3×3: `sums()` ~2 µs/frame, histogram fill ~8.6 µs/frame. **Analysis, not
  finder cost** — and now the dominant term.

### 9.1 opt6's two uses

The zero-copy path is worth having for two independent reasons, and they should be
presented as two:

1. **A faster end-to-end path for streaming consumers.** Anything that reduces as it goes
   — spectra, fitting, histogramming, writing to disk — never needs 28 M `ClusterVector`s
   resident. It needs each cluster once. For those consumers opt6 is simply the fast API,
   worth ×1.16 at 3×3 and ×2.21 at 9×9. The only consumer it excludes is one that needs
   the entire cluster list in memory at once, which at 9×9 is 9.3 GB.
2. **A profiling instrument.** Because it removes the host from the critical path
   entirely, it is the configuration in which the measured wall time *is* the GPU floor.
   That is what makes §4's rooflines checkable end-to-end rather than merely computed:
   opt6 lands on them, so the H2D ∥ kernel ∥ D2H overlap that the duty cycles claim is
   confirmed by throughput, not just by the profiler. **Use 1 to ship; use 2 to prove.**

---

## 10. Routes B′ and B″ — two rejected transports

Both attack the 467 kB/frame copy without removing it. Both lose to the allocator, for
the same underlying reason, and that reason is the argument for opt6.

### B′ — one allocation per chunk (`collect_packed`)

It removes the *per-frame* malloc but keeps the copy, and the copy is ~80 % of the cost.
Worse, the single allocation it substitutes is enormous — chunk 2500 frames ×
1422 × 328 B ≈ **1.17 GB** — far above any glibc mmap threshold, so it is `mmap`ed and
`munmap`ed every chunk and every page is faulted fresh: **606 566 faults, ~21 µs/frame.**
Many small allocations at least had a chance of heap reuse.

Measured on the like-for-like harness where all three paths produce per-cluster sums:
`collect()` 99.4 µs, **B′ 69.3**, `collect_view()` **29.9**. B′ has been **deleted from
the API**; it is documented here so the experiment is not repeated.

### B″ — parallel materialization

Before `collect_view()` existed, the obvious attack was to spread the copy over a thread
pool. Implemented, measured, reverted:

| threads | minor faults | outcome |
|--:|--:|---|
| 1 | 9 700 | reference |
| 8 | **2 270 000** | +6 % at best; **−33 %** (77 vs 57.8 µs/frame) when results were freed promptly |

The mechanism is the allocator, not the copy. Each worker thread gets its own glibc arena,
which destroys the cross-run heap reuse that makes the single-threaded path cheap —
**confirmed by `MALLOC_ARENA_MAX=1`, which collapsed 2.27 M faults to 138 k.**

> ### **The lesson both failures teach: the work is allocation-bound, not bandwidth-bound.** Copying faster does not help when the cost is the OS populating pages. The only winning move is not to allocate — which is opt6.

`materialize_slot()` is deliberately single-threaded and carries a comment recording this
([`ClusterFinderCUDA.hpp:127-134`](../include/aare/ClusterFinderCUDA.hpp#L127-L134)).

### Correctness across result paths

| path | clusters | vs CPU |
|---|--:|--:|
| CPU | 28 438 072 | — |
| CUDA per-frame | 28 445 699 | +7 627 (0.0268 %) |
| `collect()` batched | 28 447 973 | +9 901 (0.0348 %) |
| route A (CUDA Graph) | 28 447 972 | +9 900 (0.0348 %) |
| opt5 (pipelined `collect()`) | 28 447 972 | +9 900 (0.0348 %) |
| B′ `collect_packed` | 28 439 289 | +1 217 (0.0043 %) |
| **opt6 `collect_view`** | 28 447 973 | +9 901 (0.0348 %) |

opt6 is **bit-identical to `collect()`** — same device output, different transport, which
is the invariant that matters. B′'s row differs only because it runs first in its cell and
therefore sees a fresh device pedestal; on a like-for-like harness with a fresh finder per
path, all three give **identical counts** (2 847 776 over 2 000 frames), with checksums
differing by 3 × 10⁻⁷ (§13).

---
---

# ACT III — the kernel `[f32]`

*Act II ended with the host off the critical path. What is left is the GPU, and at 9×9
the kernel now stands alone.*

## 11. opt7 — the f32 device pedestal ★

### The measurement that motivates the act

At the end of Act II, `[f64]` 9×9 `s4`:

| bar | µs/frame |
|---|--:|
| **kernel** | **32.66** |
| D2H | 25.25 |
| H2D | 20.77 |

The kernel stands **9.6 µs above the next bar — 42 % taller.** By §4's rule, that 9.6 µs
is exactly what a kernel optimization can buy, and nothing before Act II could have
collected it because the host stood 31 µs taller still.

The FP64 rate on this part is **1/64 of FP32**, and the pedestal is the only f64 arithmetic
in the kernel. Dropping it to f32 is therefore the obvious move — and it needs the B1
rewrite of §11.3 to be correct at all.

### The kernel itself — `probes.csv`, `*_s1_uncontended` (exclusive, no overlap)

| config | f64 | f32 | change |
|---|--:|--:|--:|
| **9×9, 1 stream** | **39.86 µs** | **23.70 µs** | **−40.5 %** |
| 3×3, 1 stream | 14.72 µs | 4.32 µs | −70.6 % |

> ### **opt7 result: the 9×9 kernel drops 40.5 %**, nsys-verified on both builds at 20 000 frames.

### End-to-end, at the roofline

**Source**: `ladder_*.csv`, warm, both arms.

| config | opt6 `[f64]` | **opt6 + opt7 `[f32]`** | change | peak moves | vs peak |
|---|--:|--:|--:|--:|--:|
| **3×3** | 17.10 µs / 58 495 FPS | **16.31 µs / 61 312 FPS** | **−4.6 %** | 16.17 → 16.31 µs | 95 % → **100 %** |
| **9×9** | 30.01 µs / 33 323 FPS | **25.14 µs / 39 775 FPS** | **−16.2 %** | 30.01 (kernel) → 25.14 µs (**D2H**) | 100 % → **100 %** |

The mechanism is arithmetic: the probe's 9×9 floor moves 32.66 (kernel) → 25.24 µs (D2H, −23 %) and
opt6 moves 30.01 → 25.14 (−16 %). **opt6 sits on the floor, so it inherits the floor's
improvement almost exactly.**

Both beat the probe's engine-occupancy estimate — by 6.8 % at `[f64]` and 2.5 % at
`[f32]` — which is why §4 defines peak as the lower of estimate and sustained rate. Under
that definition each sits **at** its floor and the estimates corroborate it; a figure over
100 % is nonsense on its face and should never be written.

### 11.1 Why this act comes last ★

Run the identical kernel change through each earlier step's result path and it disappears:

| measured through | 3×3 f64 → f32 | 9×9 f64 → f32 |
|---|--:|--:|
| opt3 (`collect`, no overlap) | −0.4 % | +16.0 % *(+2 … +20)* |
| opt4 | −4.2 % | −5.8 % *(−17 … +12)* |
| route A (graphs) | −4.3 % | −16.9 % *(−26 … −2)* |
| **opt5** (`collect`) | −0.2 % | **−6.8 %** *(−24 … +17)* |
| **opt6** (`collect_view`) | −4.6 % | **−16.2 %** *(−16.2 … −16.2)* |

The parenthesised interval is what the two arms' own rep spreads allow: `best case` is
`min(f32)/max(f64)`, `worst case` is `max(f32)/min(f64)`. It is the honest error bar on a
difference of two best-of-warm numbers, and at 9×9 it is devastating for every allocating
path — those steps oscillate between allocator states with 12–26 % spread (§12.2), so the
interval spans 19–41 points and, except for route A, straddles zero. **The point estimates
in that column are not measurements; only `collect_view()`'s is.**

> ### **Through `collect_view()` the identical −40 % kernel is worth −16.2 %, resolvable to 0.0 points. Through every allocating path the effect cannot be measured at all.** The result path does not merely shrink the kernel win — it destroys the ability to observe it.

This is a stronger claim than the point estimates it replaces, and it survives its own
error bar. It is also the sharpest form of the ordering rule in this document: you cannot
evaluate an optimization through a stage that is itself unstable, so bottleneck order is
not merely the fastest route — it is the only order in which the intermediate results mean
anything. The 3×3 column stays quotable because 3×3 steps vary 1–3 %, not 12–26 %.

This is the single most important comparison in the document. Placed anywhere before
Act II, opt7 measures as noise and would reasonably be abandoned. Placed after, it is the
second-largest step in the ladder at 9×9. **The ordering is not presentational — it
determines whether the optimization is visible at all.**

### 11.2 opt7 also flips the regime at 3×3

| config | build | kernel | H2D | bound by |
|---|---|--:|--:|---|
| 3×3, 1 stream | f64 | **14.72** | 13.14 | **kernel** |
| 3×3, 1 stream | f32 | 4.32 | **13.15** | H2D |
| 3×3, 4 streams | f64 | 15.17 | **16.17** | H2D (barely — 1.0 µs apart) |
| 3×3, 4 streams | f32 | 5.53 | **16.63** | H2D (decisively — 11.1 µs apart) |

"opt7 does nothing at 3×3" is wrong. It does something structural: it moves 3×3 out of the
kernel-bound regime entirely, which is precisely the stated goal of the act — *keep H2D or
D2H the bottleneck*. The reason no large throughput gain appears is that the H2D bar it
lands under is the same height, so the 3×3 roofline barely moves and end-to-end barely
moves with it. Consistent, and the honest framing.

It also buys 3×3 an **occupancy step**, which is invisible in the throughput because the
kernel is no longer the binding bar. Read from the built extension with
`cuobjdump -res-usage` (see [`perf/kernel_resources.py`](../python/tests/perf/kernel_resources.py)):

| 3×3, `Cluster<int>`, 16×16 blocks | registers | spills | blocks/SM | occupancy |
|---|--:|--:|--:|--:|
| `[f64]` | 47 | 0 | 5 | **83.3 %** |
| `[f32]` | **38** | 0 | 6 | **100 %** |

The narrower accumulators free nine registers, which is exactly enough to fit a sixth
block per SM. **At 9×9 nothing moves — 128 registers on both builds** — because the
limiter there is the per-thread `clusterData[9][9]` staging array, not the pedestal
accumulators. Neither build spills.

This is worth stating explicitly because it is the one place where a register count is
*build-dependent*: any occupancy figure quoted for 3×3 must say which arm it came from.

### 11.3 opt7 is only shippable because of B1

The naive f32 pedestal produces **+28.06 % clusters and an unphysical high-energy tail** —
catastrophic cancellation in `var = sum2/n − mean²`, where both terms are ≈ 2.17 × 10⁷
while the variance is ≈ 2025, drives quiet pixels to `rms = 0` so they fire every frame.

**B1 — per-pixel offset accumulation**: freeze a per-pixel baseline `X0 ≈ round(mean)` at
t = 0 and accumulate centered `Y = X − X0`. After the rewrite, full-f32 matches f64 to
**3 × 10⁻⁷** (§13). Derivation in `docs/pedestal_precision_f32_cancellation.md`.

**Status: quotable.** Both arms same git rev, same harness, `env.json` records each.

### 11.4 Where Act III lands — and what is left

| 9×9, 4 streams | f64 | f32 | |
|---|--:|--:|---|
| **kernel** | **32.66** | 23.94 | **no longer the tallest bar** |
| D2H | 25.25 | **25.24** | identical — the slot is cap-sized, not payload-sized |
| H2D | 20.77 | 20.54 | |

At 3×3 the act achieves its goal decisively — the kernel ends 11.1 µs *below* H2D. At 9×9
it very nearly does: kernel and D2H end **5 % apart** (23.94 vs 25.24 µs). Further kernel
work at 9×9 is therefore capped at ~5 % before D2H binds instead, and the next lever there is a smaller
`cap` (§4.2) or coarser transfer granularity, not a faster kernel.

> ### **The arc, in one line: the bottleneck has been walked from the host, to the GPU, to the wire.**

---
---

## 12. Supporting study — the page-fault artifact

### Synthetic isolation

Allocating ~8 GB in ClusterVector-sized chunks (90 000 × 93 kB), touching every page,
freeing, repeating in one process:

| run | wall | minor faults |
|---|--:|--:|
| 1 (cold heap) | 2.05 s | 2 046 594 |
| 2 (warm heap) | 0.07 s | 2 232 |
| 3 (warm heap) | 0.07 s | 2 016 |

**30× faster, 1000× fewer faults**, same allocations — glibc retains the arenas.

### In situ (opt2, three consecutive executions in one process)

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
**Conclusion: the entire first-run penalty is OS page population of the host result heap.**
Persistence mode and clock ramp are not responsible.

### 12.1 The second source — pinned slot allocation (`cudaMallocHost`) ★

opt6 removes source (a) almost entirely: `collect_view()` allocates nothing per frame. That
made source (b) visible for the first time, and it was initially misread as a defect in the
zero-copy path itself.

**Symptom.** At 3×3, cap 3000, N = 20 000, the `collect_view()` loop reported **600 592
minor faults and 50.1 µs/frame (19 949 FPS)** — with the loop body reduced to `continue`.
A loop that allocates nothing cannot fault 600 k times.

**Identification, step 2 of the procedure.** At 9×9 cap 1500 (the cap in force when this
was diagnosed; it is 1700 now, 557 604 B), `m_output_bytes_per_frame` = 492 004 B. `resolve_batch_chunk` was then a pure `n/8` with no
upper bound, so handing it the whole array gave chunk = 2500:

```
2500 frames × 492 004 B  = 1.23 GB per slot
        × NUM_SLOTS (2)  = 2.46 GB
        / 4 kB           = 600 586 pages     vs 600 592 observed
```

Not a correlation — an identity. The `find_clusters_batched` path was unaffected only
because it loops in 2000-frame slices, giving 250-frame chunks and 123 MB slots.

**Fix 1 — bound the allocation.** `MAX_SLOT_BYTES = 128 MiB` now caps the auto chunk by
bytes rather than frames, so one large call behaves like a loop over slices:

```
128 MiB / 120 004 B (3×3 cap 3000) = 1118  →  rounded to n_streams  →  1120
```

**Fix 2 — move the remaining cost out of the timer.** Even bounded, the first
`submit_batch` still pins 2 × 128 MiB inside whatever region it lands in.
`reserve_output_slots(n_frames)` performs only the two `cudaMallocHost` calls — no
transfer, no launch, and critically **no pedestal advance**.

Measured, 3×3 cap 3000, N = 20 000, fresh finder each time:

| | reserve step | loop faults | loop | FPS | µs/frame | clusters |
|---|--:|--:|--:|--:|--:|--:|
| without `reserve_output_slots` | — | 67 070 | 0.387 s | 51 688 | 19.3 | 46 477 831 |
| with `reserve_output_slots` | 65.7 ms / 65 628 faults | **1 417** | 0.332 s | **60 173** | **16.6** | 46 477 831 |

Two conclusions:

- **Cost per pinned page = 65.7 ms / 65 628 = 1.0 µs**, against 0.7 µs for plain heap. The
  43 % excess is the driver work — pinning plus DMA mapping — on top of the same underlying
  first touch. A case that must additionally `cudaFreeHost` an undersized slot runs ~40 %
  higher again (measured 92 ms for the 1000 → 1120 re-allocation).
- **Cluster counts are bit-identical with and without the reserve**, confirming the call has
  no effect on results. This is what distinguishes it from the obvious alternative — running
  frames through as a warm-up — which is **invalid here**: the kernel pushes a pedestal
  update for every pixel of every frame it processes, so a warm-up leaves that finder with a
  pedestal advanced by however many frames it saw, and it can no longer be compared against
  the others (§13).

**Where it still applies.** `MAX_SLOT_BYTES` caps the *auto* chunk only; `submit_batch`
honours whatever batch size it is given. A caller submitting `BATCH_SIZE = 2000` directly
pins 2000 × 492 004 = **984 MB per slot, ~1.97 GB total** ≈ 480 k pages ≈ 480 ms at 9×9,
absorbed by whichever loop runs first.

### 12.2 Separating the two sources — the A/B that closes the model ★

Pipelined `submit_batch`/`collect`, `BATCH_SIZE = 2000`, N = 20 000, **one fresh process per
row** so nothing inherits a warm heap:

**3×3, cap 3000** (slot 229 MB × 2)

| | pre-pin | rep 0 | rep 1 | rep 2 |
|---|--:|--:|--:|--:|
| no reserve | — | 572 292 faults / 39.5 µs·f⁻¹ | 9 / 19.4 | 0 / 19.3 |
| `reserve_output_slots` | 117 192 / 120 ms | 455 129 / 34.1 | 8 / 19.5 | 0 / 19.4 |

**9×9, cap 1500** (slot 938 MB × 2 — diagnosed at the old cap; 1700 scales it to 1.06 GB)

| | pre-pin | rep 0 | rep 1 | rep 2 |
|---|--:|--:|--:|--:|
| no reserve | — | 2 759 037 / 159.8 | 292 432 / 70.8 | 292 919 / 68.9 |
| `reserve_output_slots` | 480 474 / 478 ms | 2 278 567 / 134.9 | 747 468 / 83.2 | 291 544 / 67.6 |

**The two sources are exactly additive.** Reserving subtracts precisely the pre-pin count
from run 0 and changes nothing else:

```
3×3:   572 292 −   455 129 = 117 163      vs pre-pin   117 192
9×9: 2 759 037 − 2 278 567 = 480 470      vs pre-pin   480 474
```

and both pre-pin counts equal the closed form to the digit —
`2 × 2000 × 120 004 / 4 kB = 117 191` and `2 × 2000 × 492 004 / 4 kB = 480 472`. The 3×3
heap share checks out independently: 46 477 831 clusters × 40 B / 4 kB = 453 885 against
455 129 observed, the remainder being the 20 000 `ClusterVector` objects.

Three conclusions:

1. **The heap dominates**, ~80 % of first-pass faults in both configurations (455 k of
   572 k at 3×3; 2.28 M of 2.76 M at 9×9). `reserve_output_slots` addresses the other 20 %.
   It makes run 0 honest; it does not change the plateau number that gets quoted.
2. **At 3×3 the heap faults vanish on re-run** (rep 1 → 9, rep 2 → 0). 1.86 GB per pass
   stays within what glibc retains.
3. **At 9×9 they never do — they plateau at ~292 k per pass ★.** ~9.3 GB per pass is far
   above the mmap threshold, so every pass `munmap`s the result heap and re-faults all of
   it. That is **292 k × 0.7 µs ≈ 204 ms ≈ 10 µs/frame of the 68 µs, permanently**, and no
   number of re-runs removes it. It is a floor `collect()` cannot get under.

Point 3 is an independent argument for opt6, and it explains the 14 % spread at the end of
Act I (§6): `collect_view()` allocates nothing per frame, so it removes this floor outright
rather than amortizing it.

### 12.3 What each mitigation actually removes

| | pinned slot faults | result-heap faults |
|---|---|---|
| re-running | no — dies with the finder if it is built in the timed region | yes at 3×3; **no at 9×9** (§12.2) |
| `reserve_output_slots()` | **moves them out of the timer** (still paid once) | no |
| `collect_view()` (opt6) | no — the slots are still required | **eliminates them** |
| `MAX_SLOT_BYTES` | bounds them (2.46 GB → 268 MB) | no |

Only opt6 *removes* work. The others relocate or bound it. A timed region that is genuinely
fault-free therefore needs both: `reserve_output_slots()` before the timer for source (b),
and `collect_view()` inside it for source (a) — measured at **1 417 faults on run 0 and 0
thereafter**.

---

## 13. Correctness (held constant across the whole arc)

### 3×3, N = 100 000

| finder | clusters | /frame | diff vs CPU |
|---|--:|--:|--:|
| CPU MT | 233 085 343 | 2330.85 | — |
| opt1 | 233 094 984 | 2330.95 | 0.0041 % |
| opt2 | 233 094 462 | 2330.94 | 0.0039 % |
| opt3 – opt6 (**f64**, Acts I–II) | 233 094 390 | 2330.94 | 0.0039 % |
| opt3 – opt7 (**f32**, Act III) | 233 094 465 | 2330.94 | 0.0039 % |

The residual ~0.004 % is the known per-frame vs per-pixel pedestal-update difference
(analysed in `ClusterFinderFrozen_vs_CUDA.ipynb`), **not** a precision effect.

**f64 and f32 builds differ by 75 clusters out of 233 M (3 × 10⁻⁷)** — Act III is free of
correctness cost, which is the entire point of B1 (§11.3).

### 9×9, N = 20 000

| build | clusters | /frame |
|---|--:|--:|
| f64 (opt6) | 28 447 962 | 1422.40 |
| f32 (opt6 + opt7) | 28 454 174 | 1422.71 |

### The local-max fix that made opt1/opt2 comparable

The Test3 local-max gate was backported into
[`clusterfinder_kernel_opt2.cuh`](../include/aare/clusterfinder_kernel_opt2.cuh) so
opt1/opt2 stop over-counting extended charge-shared events:

| | before | after |
|---|--:|--:|
| opt1 | 233 940 268 (2339.40/fr) | 233 094 984 (2330.95/fr) |
| opt2 | 233 931 015 (2339.31/fr) | 233 094 462 (2330.94/fr) |

−0.36 %, now matching CPU. Kernel time unchanged (the gate is an early `return`).

---

## 14. Numbers that must NOT be used

| number | why |
|---|---|
| **A cold-pass number quoted as throughput** | the `cold` column is a real measurement of a first pass, not the code's throughput. Say which you mean; at 9×9 they differ by up to 3.5× (§6, §9). |
| **The mean over reps, or the last rep, for any `collect()` step** | `collect()` oscillates between allocator states (9×9 opt4: 85.8 / 73.7 / 86.6 µs). Use best-of-warm and always print the spread. |
| **Any 9×9 `collect()` number without its fault count** | at 9×9 the result heap never reaches a zero-fault plateau (§12.2). "Warm" ≠ "fault-free" there. |
| **Any step's fault count taken from a shared-process run** | the heap is process-wide; `opt3` reports 2 faults after opt1/opt2 have run and 92 251 alone. One process per step, always. |
| **`kernel_ms` from CUDA events under multi-stream load** | queue-wait inflation up to 3.5× (§3.2). `time_kernels=False` is the default everywhere. Use nsys. |
| **Negative "PCIe + overhead"** | arithmetic artifact of `wall/N − inflated kernel`. A kernel-bound *indicator*, never a transfer cost. |
| **The `s4` kernel column as a kernel duration** | it is engine occupancy, the union of kernel intervals. f64 9×9 reads 32.66 µs at `s4` and 39.86 at `s1`. Use `s1` for "how long is the kernel" (§4.1). |
| **A probe roofline used as a hard denominator** | it is an estimate made in a loop nsys slows to ~69 µs/frame, where kernels overlap less and the union per frame reads 2–7 % high. Peak is the **lower** of it and the best sustained rate (§4). |
| **"102 % of peak"** | impossible by construction under §4's definition. If you compute one, you have used the probe estimate where the sustained rate was lower. Write *at the floor*. |
| **Any wall time from `nsys_kernel_probe.py`** | profiled: ~4× inflated by API tracing. Per-operation GPU times from probes, wall times from the ladder. |
| **Any number from a probe shorter than ~20 000 frames** | the GPU clocks never ramp, under-reading the device 7–10 % and inflating any roofline derived from it. |
| **Any duty cycle from a run on a contended GPU** | `run_ladder.py` / `run_probes.py` abort above 5 % utilisation for this reason; the f64 probe run did abort once and was re-run. |
| **9×9 numbers taken at `n_streams=8`** | 8 streams buy no kernel concurrency at 9×9 (+1 % instance time) while inflating the event timer 3.5×. The campaign fixes 4. |
| **`Speedup (CPU/CUDA)` without stating the convention** | the CPU baseline is first-pass only (`stop()` is terminal), so every speedup divides by a cold CPU and reads ~9 % generous. |
| **A CPU baseline at an unswept thread count** | the campaign used `n_threads=48` on a 16-core / 32-thread machine — 1.5× oversubscribed and 24 % slower than the best configuration. Sweep it, per cluster size: the optimum is 24 at 3×3 and 32 at 9×9 (§3.5). This one silently inflated every speedup in the deck. |
| **A CPU number timed over the loop but compared against a GPU number that includes collection** | `ladder.py` times the `ClusterCollector` drain; the notebook does not. At 9×9, 48 threads that is 2 449 FPS versus 1 338 (§3.5). |
| **Route A's ×1.03 at 3×3 as a step gain** | not established — the event tax (2.8 µs) exceeds the gap (0.8 µs), §3.2/§7. At 9×9 route A is 18 % *slower* than opt4. |
| **Any speedup measured across a heap that one loop warmed for another** | freeing a ~10 GB result heap hands it to the next loop; the first pays the whole first-touch tax. Both loops must be at plateau (§8.2). |

---

## 15. Reproduction index

> All measurement code is in [`python/tests/perf/`](../python/tests/perf/) — see its
> `README.md`. Every run writes `env.json` + `manifest.csv` into
> `perf/results/<date>_<build>/`. Reproduce both arms with `./run_campaign.sh`.

| artifact | path | role |
|---|---|---|
| **whole campaign** | `perf/run_campaign.sh` | both arms end to end, with build verification between them |
| **ladder harness** | `perf/run_ladder.py` + `ladder.py` | Acts I–III end-to-end. One process per step; one CSV row per (step, rep) |
| **probe sweep** | `perf/run_probes.py` + `nsys_kernel_probe.py` | §4. Four configs × 20 000 frames → `probes.csv` |
| **duty cycles** | `perf/gpu_span.py` | interval-union analysis of an nsys SQLite export — the only way to separate "engine busy" from "engine idle" |
| **per-operation times** | `nsys stats` on a `.sqlite` export | §4, §11. Kernel/memcpy durations without re-running the ladder. **The `.nsys-rep`/`.sqlite` pair is deliberately untracked** (`results/.gitignore`) — 0.5–2 MB each, ~40 MB per campaign — so `probes.csv` is the durable record and re-deriving means re-running `run_probes.py` |
| **shared plumbing** | `perf/common.py` | dataset, pedestal, fault bracketing, env capture, idle-GPU guard, CSV/manifest format |
| **registers / occupancy** | `perf/kernel_resources.py` | §11.2. `cuobjdump -res-usage` on the built extension + the sm_89 occupancy arithmetic — no rebuild needed |
| 3×3 results | `perf/results/2026-08-18_{f64,f32}/` | `ladder_3x3.csv`, `ladder_9x9.csv`, `probes.csv` — 9×9 rows here are **cap 1500**, superseded |
| 9×9 results | `perf/results/2026-08-20_{f64,f32}_cap1700/` | `ladder_9x9.csv` at the lossless cap — every 9×9 end-to-end number |
| 9×9 engine times | `perf/results/2026-08-20_{f64,f32}_capAB/` | `probes.csv`, both caps × {s1, s4} in one session |
| opt7 build axis | `include/aare/clusterfinder_kernel.cuh` lines 16–17 | `COMPUTE_TYPE` / `DEVICE_PED_TYPE` |
| opt1/opt2 class | `include/aare/ClusterFinderCUDAOpt2.hpp` | frozen pre-refactor pipeline; gained `time_kernels` for comparability |
| opt3 – opt7 | `include/aare/ClusterFinderCUDA.hpp`, `clusterfinder_kernel.cuh` | current pipeline |
| route A | `include/aare/ClusterFinderCUDA_graph.hpp` | graph-based finder (rejected) |
| exploratory notebook | `python/tests/ClusterFinderCUDA_perf.ipynb` | **stores only the last run** — not a record. Archive a copy per cluster size if used |
| correctness notebook | `python/tests/ClusterFinderFrozen_vs_CUDA.ipynb` | CPU↔CUDA agreement analysis |
| precision study | `docs/pedestal_precision_f32_cancellation.md` | B1 derivation |
| deck | `docs/cf_cuda_performance.pptx` + `docs/deck/build_performance_deck.py` | 35 slides in the same three acts plus a 7-group annex (A1–A7, A7 in two parts), 55 pages with dividers; figures from `docs/deck/make_figs.py` and `make_figs_kernel.py`. Rebuild with `python docs/deck/make_figs.py && python docs/deck/build_performance_deck.py` |

### CSV step labels

The harness predates this document's step names. To read a `ladder_*.csv` row:

| `step` column | step in this document |
|---|---|
| `cpu` | baseline |
| `opt1` · `opt2` · `opt3` · `opt4` | opt1 · opt2 · opt3 · opt4 |
| `opt5` | **route A** (CUDA Graphs, rejected — §7) |
| `opt7` | **opt5** (chunked host↔GPU overlap — §8) |
| `opt8` | **opt6** (zero-copy collection — §9) |
| *(build axis, not a row)* | **opt7** — `[f32]` vs `[f64]` directory (§11) |

### Reading a profile without re-running it

Every probe ships its SQLite export, so all of §4 can be recomputed offline on an idle
or busy machine — no GPU, no rebuild.

```bash
cd python/tests/perf
NSYS=/opt/nvidia/nsight-systems/2024.5.1/bin/nsys

# per-operation durations, straight from the recorded profile
$NSYS stats --report cuda_gpu_kern_sum --report cuda_gpu_mem_time_sum \
    --format table results/2026-08-18_f64/probe_9x9_s1_uncontended.sqlite

# duty cycles, overlap and the derived roofline
python gpu_span.py results/2026-08-18_f64/probe_9x9_s4.sqlite 20000
```

The first command reproduces §11's kernel claim directly: 20 000 instances,
**avg 39 862 ns**, with D2H 21 969 and H2D 13 204 ns — the 39.86 / 21.97 / 13.20 row.

Three things to know:

1. **Pass the `.sqlite`, not the `.nsys-rep`.** The committed exports are older than
   their reps, so the rep path refuses to run without `--force-export=true`, which
   silently rewrites the export `gpu_span.py` reads.
2. **`gpu_span.py` needs the frame count as its second argument** (always `20000`
   here). It is the divisor and is not stored in the profile.
3. **`nsys stats` cannot produce a roofline.** It reports per-operation *sums and
   averages*, which cannot distinguish "the engine was busy" from "the engine was idle
   waiting". It answers *how long is one kernel* — the `s1` column and the opt7 −40.5 %
   — while duty cycle, `overlap` and the roofline need the interval-union pass of
   `gpu_span.py` (§4.1). This is why the same f64 9×9 `s4` run reads 32.66 µs of kernel
   occupancy while `nsys stats` reports **43.2 µs** per kernel instance (863.51 ms of
   summed duration over 20 000 launches): under contention each kernel is *slower* than
   its 39.86 µs `s1` duration, and only the 1.32× self-overlap pulls the per-frame
   occupancy below either figure.

Other useful reports: `cuda_gpu_trace` (every operation with timestamps),
`cuda_gpu_mem_size_sum` (transfer bytes). `$NSYS stats --help-reports` lists them all;
`--format csv` pipes cleanly.

### Protocol to reproduce a clean number

1. **Idle GPU.** Both drivers abort above 5 % utilisation — close any notebook. A competing
   process leaves per-op averages intact while destroying the duty cycle.
2. `./run_campaign.sh`. The arm is one line in the kernel header; the script verifies the
   rebuild actually took effect before recording anything, and restores the f32 default at
   the end.
3. **Quote the warm column, and print the spread.** Cold is a real number too — label it.
4. For kernel times use the **`s1`** probe (exclusive); for "% of peak" use **`s4`** (the
   ladder's configuration), and prefer the f32 arm for percentages (§9).
5. Leave `time_kernels=False` everywhere. It is the default on all three finders.
6. Never warm up by processing frames — the kernel advances the pedestal per frame.
   `reserve_output_slots()` pre-pays the pinned allocation without touching it.

---

## 16. Slide-ready takeaways

Every number here is from `[f32]`/`[f64]` (§0) and reproducible with
`perf/run_campaign.sh`.

### The arc

1. **One rule explains the whole deck**: an optimization buys the distance between the bar
   it attacks and the next-tallest bar. It predicts every result, including the three that
   look like failures.
2. **The ladder is ordered by that rule.** Feed the GPU (Act I), get the results back
   (Act II), then — and only then — make the GPU faster (Act III). Each act removes the
   constraint that makes the next one measurable.
3. **The bottleneck is measured, not assumed.** At 3×3 the kernel is 5.5 µs against a
   16.6 µs frame upload, with the copy engine **72.7 % busy** and the kernel 24.2 %. At 9×9
   the kernel is the tallest bar. Same code, opposite regimes.

### Act I — feeding the GPU `[f64]`

4. **3×3: 1.00× → 2.34× → 3.66× → 4.32× → 5.69×** over the best CPU configuration
   (24 threads), at identical correctness (4 × 10⁻⁶). The kernel never changed; what
   shrank was everything around it, 148 → 26 µs/frame.
5. **opt4 (pinned input) is worth 1.32× at 3×3 and 1.03× at 9×9** — the rule's first
   confirmation. Pinning attacks H2D, which is the tallest bar at 3×3 and the shortest at
   9×9.
6. **Act I ends at 62 % of peak at 3×3 and 38 % at 9×9.** The GPU is idle much of the
   time and the whole remainder is host-side.

### Act II — getting the results back `[f64]`

7. **opt5 — chunked host↔GPU overlap is ×1.31 at 3×3 and ×1.20 at 9×9**, for an API change
   with no CUDA work at all. It is now internal to `find_clusters_batched()`, so callers get
   it free.
8. **opt6 — zero-copy collection puts both configurations on their floor**: 3×3 reaches 95 %
   of its 16.17 µs H2D peak, 9×9 goes from 45 % to the floor itself (**×2.21**).
   Bit-identical results to `collect()`.
9. **The win from zero-copy is `max(0, host_copy − gpu_floor)`** — ×2.21 at 9×9 (467 kB/frame
   ≈ 40 µs of copy against a 30 µs floor: cannot hide) and ×1.16 at 3×3 (93 kB ≈ 8 µs against
   16 µs: hides completely). Rule 1, applied to the host.
10. **opt6's real advantage is reproducibility.** Its spread over 5 reps is **0.2 %**; every
    path that allocates per frame varies **1–25 %**. A 25 % spread means the number depends
    on which run you quote.
11. **opt6 has two uses**: the fast path for any streaming consumer (spectra, fitting, disk),
    and the profiling instrument that proves the GPU floor is real — it is the only
    configuration whose wall time *is* the roofline.

### Act III — the kernel `[f32]`

12. **Act II is what makes Act III measurable.** At the end of Act II the f64 9×9 kernel
    stands 9.6 µs above the next bar; before Act II the host stood 31 µs above the kernel.
13. **opt7 (f32 pedestal) cuts the 9×9 kernel 40.5 %** — 39.86 → 23.70 µs, nsys-verified on
    both builds — and delivers **−16.2 % end-to-end** (30.01 → 25.14 µs / 39 775 FPS). It
    lands on the **D2H** floor, not the kernel floor: the cut took the kernel *under* the
    25.24 µs result path, so the constraint changed engine.
14. **Through `collect()` the same −40 % kernel cannot be measured at all.** Its 9×9 rep
    spread (22–26 %) puts the effect anywhere in −24 … +17 %; through `collect_view()` the
    same quantity is −16.2 % with a 0.0-point interval. The ordering does not just decide
    how large the win looks — it decides whether the measurement means anything.
15. **opt7 is only shippable because of the B1 variance rewrite** — the naive f32 pedestal
    gives +28 % clusters and an unphysical tail; centered accumulation restores agreement
    with f64 to 3 × 10⁻⁷.
16. **opt7 also flips the regime at 3×3**, moving it out of kernel-bound entirely (f64 s1:
    kernel 14.72 > H2D 13.14; f32 s1: kernel 4.32 ≪ H2D 13.15). That *is* the goal of the act
    — keep the transfers binding.

### Discipline, and what is next

17. **Three step ideas were measured and rejected**, and the rule predicts all three: **CUDA
    Graphs** attacked launch overhead, which stops binding one step later (18 % *slower* than
    opt4 at 9×9); **one-allocation-per-chunk** and **parallel materialization** attacked a
    copy that is allocation-bound, not bandwidth-bound.
18. **Measurement discipline was necessary to get here**: first-touch page faults from two
    independent allocators (heap 0.7 µs/page recurring, `cudaMallocHost` 1.0 µs/page one-off),
    CUDA-event queue-wait inflation up to 3.5× (which is why event removal is *methodology*,
    not a rung), per-step heap contamination (`opt3`: 2 faults in-process vs 92 251 isolated),
    and clock ramp on short probes under-reading the GPU 10 %.
19. **The bottleneck has been walked from the host, to the GPU, to the wire.** What is left:
    - **3×3**: the 16.63 µs achieved-config H2D floor sits 3.5 µs above the 13.15 µs
      uncontended rate — transfer *granularity* (2 000 separate 320 kB descriptors) plus
      H2D↔D2H contention costing 26 %.
    - **9×9**: at the lossless cap D2H **already binds** — 25.24 µs against a 23.94 µs
      kernel. Further kernel work buys nothing at all; the lever is the cluster cap
      (§4.2), not the kernel.
    - **Both**: the largest remaining per-frame cost is downstream *analysis*, not the finder
      — histogram fill is ~8.6 µs/frame at 3×3, over half the 16.63 µs budget.
