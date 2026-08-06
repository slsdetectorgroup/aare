# ClusterFinderCUDA — Optimization Recap

A step-by-step walkthrough of the optimizations applied to `ClusterFinderCUDA`,
in the order they were implemented. Each section = one slide: what changed,
why, and what it bought us.

**Setup** (constant across all measurements below):
- GPU: RTX 4090 (Ada, sm_89), PCIe 4.0 x16
- Detector: 400×400, `uint16` frames
- Cluster size: 3×3, `nSigma = 5`
- Reference: single-threaded CPU `ClusterFinder`

| Reference                  | ms / frame |
|-----------------------------|-----------:|
| CPU (single-threaded)       | ~1.7 – 1.8 |

---

## Step 0 — First CUDA port: single-frame `find_clusters()`

The very first GPU path: one frame in, H2D copy → kernel → D2H copy, fully
synchronous, no streams.

```cpp
cuda_cf.find_clusters(frame, frame_number);
```

| Metric                                   | ms / frame |
|-------------------------------------------|-----------:|
| GPU, single frame, single stream           | **0.057 – 0.062** |

**Already ~28–30× faster than CPU**, just from moving the stencil to the GPU —
before any pipelining work begins. This number stays roughly constant
throughout the rest of the journey: it's the "no batching, no overlap" floor
that the batched path is measured against.

---

## Step 1 — Multi-stream batched cluster finder

*Commit `3ed773e` — Add multi-stream ClusterFinderCUDA with batched processing*

- Introduced `StreamContext`: each CUDA stream owns its own device buffers
  and pedestal arrays
- `find_clusters_batched()`: N frames distributed round-robin across
  `n_streams` streams
- Split `ClusterFinderCUDA.cuh` into the host wrapper (`ClusterFinderCUDA.hpp`)
  and the device kernel (`clusterfinder_kernel.cuh`)

```cpp
struct StreamContext {
    cudaStream_t stream;
    FRAME_TYPE  *d_frame;
    ClusterType *d_clusters;
    uint32_t    *d_cluster_count;
    PEDESTAL_TYPE *d_pd_mean, *d_pd_sum, *d_pd_sum2;
};
std::vector<StreamContext> m_streams;
```

This is the scaffolding everything else builds on — no isolated benchmark
yet, but it's what makes "more streams = more overlap" possible in Step 4.

---

## Step 2 — Mixed precision: FP32 stencil, FP64 pedestal

*Commit `ac96d1f` — Implement mixed precision: f32 stencil, f64 pedestal*

```cpp
// clusterfinder_kernel.cuh
using COMPUTE_TYPE = float;   // stencil arithmetic + shared memory

auto load_pixel = [&] __device__(ssize_t gr, ssize_t gc) -> COMPUTE_TYPE {
    auto gid = gc + ncols * gr;
    return static_cast<COMPUTE_TYPE>(d_frame[gid])
         - static_cast<COMPUTE_TYPE>(d_pd_mean[gid]);
};
```

**Why:**
- FP32 throughput on Ada is ~64× FP64 — the per-pixel stencil sum/max/quadrant
  reductions don't need double precision
- `float` shared memory (stride-18 tile) maps to distinct banks; the previous
  `double` layout caused bank conflicts

Pedestal accumulation (`mean`/`sum`/`sum2`) stays `double` at this stage —
that's addressed in Step 5. No isolated number here; this change is a
prerequisite for the kernel speedup measured later.

---

## Step 3 — Per-frame kernel timing via CUDA events

*Commit `34e69a8` — Add per-frame kernel timing via CUDA events*

```cpp
cudaEventRecord(m_kernel_start_pool[slot], stream);
find_clusters_in_single_frame<<<grid, block, smem, stream>>>(...);
cudaEventRecord(m_kernel_stop_pool[slot], stream);
...
float ms = 0;
cudaEventElapsedTime(&ms, start, stop);
```

This is purely **instrumentation** — it adds `avg_kernel_time_ms()`, which
isolates *kernel-only* time from PCIe transfer time. Every "kernel: X µs"
number quoted from here on comes from this pool. No performance change, but
without it Step 5's "3.3× kernel speedup" would be invisible (it's masked by
PCIe in the wall-clock total).

---

## Step 4 — Eliminate sync barriers + pinned transfers

*Commits `88e0e8d` (transfer/kernel hot path) + `6a12e3d` (refactor)*

**88e0e8d:**
- Per-stream **pinned host staging buffers** for truly async H2D/D2H
- Stop reserving full device capacity per output frame — delay cluster
  payload construction until a candidate is confirmed
- Replace per-pixel `sqrtf()` threshold checks with squared comparisons

**6a12e3d:**
- Replace **one `cudaStreamSynchronize` per frame** with **one per stream per
  batch** — cuts sync calls from `O(n_frames × n_streams)` to `O(n_streams)`
- Unified D2H output layout `[uint32_t count | clusters[max]]` in one
  lazily-allocated pinned pool
- New `register_input_buffer()` / `unregister_input_buffer()` — pin the
  caller's batch buffer once via `cudaHostRegister`, so every
  `find_clusters_batched()` slice transfers at DMA speed (~22 GB/s) instead of
  ~15 GB/s for pageable memory

```cpp
// continuous-queue launch loop (no per-frame sync)
for (auto &sc : m_streams) {
    for (frame : frames_for_this_stream) {
        cudaMemcpyAsync(sc.d_frame, ..., cudaMemcpyHostToDevice, sc.stream);
        find_clusters_in_single_frame<<<..., sc.stream>>>(...);
        cudaMemcpyAsync(h_output_pinned[slot], sc.d_clusters, ..., sc.stream);
    }
}
for (auto &sc : m_streams) cudaStreamSynchronize(sc.stream);  // once per stream
```

| Configuration                        | ms / frame |
|----------------------------------------|-----------:|
| Batched, 1 stream                       | 0.052      |
| Batched, 5 streams — **before** refactor | ~0.034     |
| Batched, 5 streams — **after** refactor  | **0.028**  |

**−18%** at 5 streams, purely from removing host-side sync barriers and
making transfers DMA-speed. Going from 1→5 streams alone (52→28 µs) shows the
multi-stream scaffolding from Step 1 finally paying off once the host isn't
serializing on every frame.

---

## Step 5 — FP32 device pedestal + bulk memcpy drain

*Commit `4c66802` — FP32 device pedestal and bulk memcpy drain*

```cpp
// device pedestal arrays: double -> float
float *__restrict__ d_pd_mean, *__restrict__ d_pd_sum, *__restrict__ d_pd_sum2;

float var_px = d_pd_sum2[global_tid] / static_cast<float>(n_pd_samples)
             - mean_px * mean_px;
float rms_sq = fmaxf(var_px, 0.0f);
```

```cpp
// D2H drain: per-cluster push_back loop -> single resize + memcpy
results[frame_idx].resize(n_found);
std::memcpy(results[frame_idx].data(), src, n_found * sizeof(ClusterType));
```

| Metric                       | Before | After  | Change |
|-------------------------------|-------:|-------:|-------:|
| Kernel-only time               | 15 µs  | 4.6 µs | **3.3×** |
| Batched, 5 streams (overall)   | 28 µs  | 26 µs  | ~7%    |

**Why the kernel got 3.3× faster but the overall number barely moved:**
this is Amdahl's law in action. At 28 µs total, the kernel was already a
small slice — PCIe H2D/D2H dominates. Cutting the kernel to a quarter of its
size can only remove *the kernel's share* of the time. The big remaining
lever is PCIe, not compute — which motivates Step 6.

---

## Step 6 — Async `submit_batch()` / `collect()` pipeline

*Commit `5922c73` — async submit_batch/collect API*

```cpp
auto tok = cf.submit_batch(buf_a, first_frame=0);      // enqueue, don't wait
for (...) {
    buf_b[:n] = data[start:start+n];                   // CPU fills next buffer
    auto next_tok = cf.submit_batch(buf_b, first_frame=start);
    results += cf.collect(tok);                        // GPU runs buf_b meanwhile
    tok = next_tok;
    std::swap(buf_a, buf_b);
}
results += cf.collect(tok);
```

- `submit_batch()`: enqueues H2D + kernel + D2H, returns a `BatchToken`,
  **never blocks**
- `collect()`: waits on a `cudaEvent` (not `cudaStreamSynchronize`) so a
  second batch already queued behind the first keeps running

| Metric                       | ms / frame |
|---------------------------------|-----------:|
| Batched, 5 streams (Step 5)      | 0.026      |
| **Async pipeline**                | **0.022**  |

**−15–18%.** The mechanism: for `BATCH_SIZE=2000` (640 MB/batch), the CPU
memcpy that fills the next buffer (~25 ms) is *longer* than the GPU's batch
execution time (~12 ms). The async pipeline hides the entire GPU batch inside
that CPU memcpy window — the GPU is no longer idle while the CPU prepares the
next chunk.

---

## Summary — the full journey

| Stage                                  | ms / frame | Speedup vs CPU |
|------------------------------------------|-----------:|---------------:|
| CPU (reference)                            | 1.7 – 1.8  | 1×             |
| GPU, single frame (Step 0)                 | 0.057      | ~30×           |
| Batched, 1 stream (Step 1)                 | 0.052      | ~33×           |
| Batched, 5 streams, sync-barrier removed (Step 4) | 0.028 | ~63×       |
| + FP32 device pedestal (Step 5)            | 0.026      | ~67×           |
| + Async pipeline (Step 6)                  | **0.022**  | **~78×**       |

```
1.8 ms ──────────────────────────────────────────────────► CPU
0.057 ms ──────► GPU naive
0.052 ms ─────► batched, 1 stream
0.028 ms ───► sync-barrier removal + pinned transfers
0.026 ms ───► FP32 pedestal (kernel 3.3×, overall ~flat — Amdahl)
0.022 ms ──► async submit/collect pipeline
```

---

## What's next

See `optimization_summary.md` for the original profiling-driven roadmap.
Remaining levers, roughly by expected impact:

1. **GPU-side cluster compaction before D2H** — D2H currently moves
   `max_clusters_per_frame` slots regardless of occupancy
2. **Larger batch size** — amortize per-batch launch overhead further
3. **Smaller `Cluster` struct** — reduce D2H payload per cluster
4. **GPUDirect RDMA** — eliminate H2D entirely if data arrives via
   InfiniBand/RoCE
