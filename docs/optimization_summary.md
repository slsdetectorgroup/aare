# ClusterFinder CUDA — Profiling, Bottleneck Analysis, and Optimization Roadmap

## 1. Setup

- **Hardware**: RTX 4090 (Ada, sm_89), PCIe 4.0 x16
- **Workload**: 50,000 frames of 400×400 uint16 detector data, 3×3 cluster finding
- **Tooling**: Nsight Systems 2024.5.1 (system-wide timeline), Nsight Compute 2024.1.1 (per-kernel deep dive)
- **Profiler counter access** unlocked via:
  ```
  /etc/modprobe.d/nvidia-profiler.conf:
      options nvidia NVreg_RestrictProfilingToAdminUsers=0
  ```
  Confirmed by `cat /proc/driver/nvidia/params | grep -i profil` → `RmProfilingAdminOnly: 0`.

---

## 2. Bottlenecks Identified from Nsys Profiling

### 2.1 Overall wall-time vs. ideal floor

From the original Phase 4 run (50k frames, 5 streams, BATCH_SIZE=2000):

| Resource | Total time | Phase 4 utilization |
|---|---|---|
| Kernel execution | 824 ms | 21.1% |
| H2D copy engine | 746 ms | 19.1% |
| D2H copy engine | 235 ms | 6.0% |
| **Phase 4 wall time** | **3903 ms** | — |

These three engines are independent hardware resources. With perfect overlap, the wall-clock floor is `max(824, 746, 235) ≈ 824 ms`. Actual measured wall time: 3903 ms. **The GPU was idle ~80% of the time.** This is host-scheduling-limited, not GPU-compute-limited.

### 2.2 Specific causes

**Excessive host-device synchronization.** 110,029 `cudaStreamSynchronize` calls (~2.2 per frame) consuming 452 ms of host time. Each sync blocks the host scheduler thread and prevents queueing of subsequent stream commands.

**Two D2H transfers per frame.** 110,002 D2H operations vs. 55,001 kernel launches — a count-then-data pattern where the second D2H size depends on the first D2H's result, forcing a per-frame sync.

**Round-barrier pattern in the launch loop.** Code structure `for round { launch 5 streams; sync 5 streams }` forces stream 0 to wait for the slowest stream in each round. PCIe RX throughput visibly pulses with ~80 µs busy / ~40 µs idle in the timeline, confirming the H2D engine isn't continuously fed.

**Redundant host memcpy.** The call site copies into a pageable `batch_buffer`, then `find_clusters_batched` copies *again* into per-stream pinned buffers. ~1.25 GB of host-DRAM memcpy per batch, all on one CPU core, blocking the launch loop. This shows up in the timeline as ~50–70 ms gaps between bursts where the GPU is fully idle.

**Per-frame launch overhead at small kernel size.** Each frame triggers ~18 µs of fixed CUDA API overhead (launch + 3 memcpys + 2 syncs), vs. only ~15 µs of actual kernel execution. The host scheduling cost dominates the kernel cost.

### 2.3 What was working

PCIe pinned-memory transfers measured at 23.7 GB/s sustained (~95% of PCIe 4.0 x16 practical peak), confirming `cudaMallocHost` allocations are truly pinned. The kernel itself is regular and well-behaved (4% stddev across 55k launches, no thermal throttling, no spills per `ptxas` output: `0 bytes spill stores, 0 bytes spill loads`).

---

## 3. Three-Tier Optimization Plan

### Tier 1 — Recover the missing 2 seconds (host-side fixes)

Largest expected impact. Targets the gap between 824 ms (ideal floor) and 3903 ms (measured) by fixing host scheduling and synchronization patterns.

- **Drop the round-barrier**: continuous queueing across all streams, with a single sync per stream at end-of-batch
- **Collapse count-then-data D2H into a single fixed-size D2H**: per-frame device output as one contiguous `[count][clusters[max]]` block, eliminating the per-frame sync dependency
- **Eliminate the redundant host memcpy**: ClusterFinder owns one large pinned staging buffer, populated by a single bulk memcpy at the top of `find_clusters_batched`
- **Expected gain**: Phase 4 wall time from 3903 ms → ~1000–1500 ms

### Tier 2 — Per-launch overhead reduction

After Tier 1 closes the host-scheduling gap, the next bottleneck becomes the per-launch CUDA API cost.

- **True work coalescing**: one kernel processes N frames via `blockIdx.z`, amortizing the 2.5 µs `cudaLaunchKernel` cost over N×. Requires kernel signature change and a decision on how to handle per-frame pedestal updates (likely: move pedestal update to a separate sparse kernel)
- **CUDA Graphs**: capture the per-batch sequence (memset → H2D → kernel → D2H) and replay as a single command-stream submission
- **Expected gain**: ~1.5–2× over Tier 1, approaching the H2D copy engine floor (~13.5 µs/frame, ~675 ms for 50k frames)

### Tier 3 — Kernel internals

Worth doing only after Tier 1 and 2, and only after `ncu --set full` confirms which kernel-internal change matters.

- **Drop the per-pixel double-precision arithmetic** for variance/threshold (Ada has 1/64 FP64 throughput; precompute thresholds as float during pedestal updates)
- **Pedestal update write-back traffic**: most pixels are non-photons and write 3 × 8 bytes per pixel, dominating DRAM traffic. Either move to float, or defer pedestal updates to a separate kernel
- **Halo-loading divergence cleanup**: replace the role-based corner-handling code with a uniform cooperative tile load
- **Block geometry**: A/B test 32×8 vs 16×16 once everything else is locked in

---

## 4. What Was Implemented (Tier 1)

### Changes to `ClusterFinderCUDA.hpp`

**New constructor signature**:
```cpp
ClusterFinderCUDA(Shape<2> shape, COMPUTE_TYPE nSigma,
                  size_t max_clusters_per_frame,   // was: capacity (global)
                  int n_streams,
                  size_t staging_batch_capacity)    // new
```
The third argument's semantics changed from "global cluster buffer size per stream" to "tight upper bound per frame." This is now a real number used for the fixed-size D2H, not an opaque global capacity.

**Per-frame device output as single block**:
```
d_output layout: [uint32_t count][padding to ClusterType alignment][ClusterType clusters[max]]
```
Offset computed as `(count_bytes + cluster_align - 1) & ~(cluster_align - 1)` for power-of-two alignment safety.

**Internal pinned staging buffer**: one `cudaMallocHost` of `staging_batch_capacity × image_bytes`, owned for the lifetime of the ClusterFinder. The caller's `NDView` is bulk-copied into this buffer once per batch.

**Continuous-queue launch loop**: frames distributed round-robin across streams; per-stream nested loop queues all of a stream's frames back-to-back without intermediate syncs. Single `cudaStreamSynchronize` per stream at end of batch.

**Lazy-grown pinned output pool**: one slot per frame within a batch (`n_frames × output_bytes_per_frame`), so each frame's D2H targets its own pinned destination and frames don't overwrite each other before parsing.

**Per-frame kernel event timing pool** (restored after initial removal): one event pair per frame slot, allocated lazily up to `staging_batch_capacity`, enables accurate `avg_kernel_time_ms()` reporting without the aliasing problem that one-event-pair-per-stream would have in continuous-queue mode.

### Changes to bindings (`bind_ClusterFinderCUDA.hpp`)

- Constructor binding updated to the new 5-argument form with renamed `max_clusters_per_frame` and new `staging_batch_capacity`
- `steal_clusters()` no longer takes the `realloc_same_capacity` bool argument
- `steal_clusters()` lambda uses `std::move` to transfer ownership out of the internal reference (required because `ClusterVector` is move-only)
- Re-added `#include <pybind11/stl.h>` for automatic `std::vector` return conversion
- Added `py::array::c_style | py::array::forcecast` to all `py::array_t` parameters to enforce contiguity at the binding boundary

### Changes to Python factory (`ClusterFinderCUDA` function)

- Kwarg `capacity` → `max_clusters_per_frame`
- New kwarg `staging_batch_capacity` (defaults to 2000)
- Default `n_streams` raised from 1 to 4
- Tighter default `max_clusters_per_frame=2048` (was unbounded-feel with `capacity=1024` that would have silently truncated)

### Changes to test/benchmark code

- Constructor call sites updated to new signature
- `steal_clusters(true)` → `steal_clusters()` (no bool argument)
- Optional: out-parameter overload of `find_clusters_batched` to avoid per-batch result vector allocation in tight benchmark loops

### Changes to Jupyter notebook

- Two `steal_clusters(realloc_same_capacity=...)` calls in the non-batched branch updated to `steal_clusters()`
- BATCHED branch unchanged

---

## 5. What Still Needs to Be Done

### Immediate next step: validate Tier 1

Re-run the test binary and check:
- Phase 4 wall time should drop from 3903 ms → ~1000–1500 ms
- `cudaStreamSynchronize` count in `nsys` stats should drop from 110k → ~`n_batches × n_streams` (~125 for 25 batches × 5 streams)
- `avg_kernel_time_ms()` from the restored event pool should report ~0.015 ms (15 µs)
- Per-frame wall time should land between 20–30 µs (vs. old 78 µs)

If Phase 4 is significantly above ~1500 ms, the next thing to investigate in `nsys` is whether the host bulk-memcpy at the top of `find_clusters_batched` is on the critical path — visible as a gap at the start of each batch in the timeline.

### Tier 1.5 — overlap host-memcpy with GPU execution

The current implementation runs the bulk host-memcpy on the calling thread, blocking the launch loop. To hide this cost:

- **Worker-thread approach**: dedicate one host thread to filling staging buffer N+1 while the GPU consumes batch N. Requires a ping-pong (or larger) staging buffer.
- **Ping-pong staging**: two pinned staging buffers, alternating between them per batch
- **Cost**: adds threading complexity, but the bulk memcpy (~50 ms per 2000-frame batch) is currently on the critical path; hiding it could close the remaining gap to the kernel floor

### Tier 2 — Work coalescing and CUDA Graphs

Pending Tier 1 validation. The two pieces:

**Multi-frame kernel via `blockIdx.z`**:
- New kernel signature accepting `[n_frames, nrows, ncols]` input and per-frame output blocks
- Resolve the pedestal-update race: simplest option is to disable the in-kernel pedestal update and run a separate sparse update kernel once per batch (small mathematical difference from current frame-by-frame behavior, likely negligible for slowly-drifting pedestals)
- Estimated gain: collapses 2000 launches per batch into ~5–10 launches; saves ~120 ms of host overhead across Phase 4

**CUDA Graph capture**: once the per-batch sequence is regular (same operations on same buffers each time), capture once and replay. ~1.5–2× on launch-bound workloads. Synergizes with the multi-frame kernel because both reduce per-frame host overhead.

### Tier 3 — Kernel internal optimization

Requires `ncu --set full` profiling run on a single kernel launch to prioritize:
- Source counters → confirm 0 register spills
- Memory Workload Analysis → L1 hit rate (current 3×3 stencil should benefit from shared memory; check whether it actually does)
- Warp State Statistics → identify dominant stall reason (Long Scoreboard = DRAM-bound; Short Scoreboard = shared-memory-bound; Wait = barrier-bound)
- GPU Speed of Light / Roofline → confirm kernel is memory-bound (expected) and how far from the DRAM ceiling

Then prioritize the kernel-internal changes (FP64 reduction, pedestal update offload, halo-load cleanup, block geometry) based on which one the profiler indicates as binding.

### Open correctness issue (independent of perf work)

The original Phase 5 comparison reported **13.7M `data_mismatch` clusters and ~0.5M each of CPU-only / GPU-only** out of 69M total. This is a kernel-vs-CPU algorithmic difference, not a performance issue, but worth tracing before further kernel optimization. Likely candidates:

- Tie-breaking rule between adjacent local maxima (CPU and GPU may disagree on which pixel "wins" when two neighbors have equal values)
- Off-by-one in cluster window indexing at frame boundaries
- Difference in whether the central pixel must be a strict max (`>`) vs. ≥ max (`>=`)
- The kernel's pedestal-update timing (CPU updates per-pixel sequentially as it walks the frame; GPU updates the entire frame's pedestal in parallel after the cluster decisions)

Profile a correct kernel, not a fast wrong one.

---

## 6. Reference Numbers

For future "did we improve?" comparisons:

| Metric                        | Original  | Tier 1 target | Tier 2 target | Theoretical floor  |
|-------------------------------|-----------|---------------|---------------|--------------------|
| Phase 4 wall time             | 3903 ms   | ~1200 ms      | ~700 ms       | 675 ms (H2D-bound) |
| Per-frame wall                | 78 µs     | 24 µs         | 14 µs         | 13.5 µs            |
| Per-frame kernel              | 15 µs     | 15 µs         | 15 µs         | 15 µs              |
| `cudaStreamSynchronize` count | 110,029   | ~125          | ~25           | —                  |
| `cudaLaunchKernel` count      | 55,001    | 55,001        | ~125          | —                  |
| H2D bandwidth                 | 23.7 GB/s | 23.7 GB/s     | ~25 GB/s      | 26 GB/s (PCIe 4.0 x16 pinned) |

The H2D engine is the hard floor on this hardware. To go below 13.5 µs/frame requires either reducing PCIe payload (uint8 input → 2× faster) or doing more compute per frame transferred (currently the simplest finder; nothing else to amortize over).
