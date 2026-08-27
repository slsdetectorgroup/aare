# Questions the room will ask

Answers worked out while building `docs/cf_cuda_performance.pptx`, kept because most
of them came from someone reading a slide and not believing it. Each one names the
slide it belongs to and the code or measurement that settles it.

Conventions used throughout: **s1** is one stream (true exclusive engine durations),
**s4** is the shipped four-stream pipeline (engine *occupancy*), and **floor** is
`1 / max(H2D, kernel, D2H)` at s4 — 30.01 µs/frame = 33 323 FPS at 9×9. Numbers come
from `docs/ClusterFinderCUDA_benchmark_results.md`; section references are to it.

---

## 1 · The result path — D2H, pinned buffers, chunks and slots

### Q. Where do 93 kB/frame (3×3) and 467 kB/frame (9×9) come from?

They are `clusters actually found × sizeof(ClusterType)`, per frame, averaged over the
run. Neither is a measured byte count from a profiler; both are arithmetic on the
cluster struct.

`Cluster` is two `CoordType` coordinates plus `std::array<T, X*Y>`. With 16-bit coords
and `int32` data that is `4 + 9×4 = 40 B` at 3×3 and `4 + 81×4 = 328 B` at 9×9 —
`alignof` 4, no tail padding, so `sizeof` is exact.

| | cap | fill | clusters/frame | × sizeof | payload |
|---|--:|--:|--:|--:|--:|
| 3×3 | 3000 | 77 % | ~2310 | × 40 B | **~93 kB** |
| 9×9 | 1700 | 84 % | ~1428 | × 328 B | **~467 kB** |

The fills are the campaign occupancies printed in §8 — 77 % at cap 3000, 84 % at
cap 1700.

### Q. So is 93 kB the size of the D2H transfer?

**No, and this is the distinction worth making out loud.** There are two different
byte counts on this path and they are not close.

- **The D2H is occupancy-blind.** The device writes into a fixed envelope and the
  copy moves the whole envelope regardless of how many clusters were found:

  ```cpp
  m_clusters_offset   = align_up(sizeof(uint32_t), alignof(ClusterType));   // = 4
  m_output_bytes_per_frame = m_clusters_offset
                           + m_max_clusters_per_frame * sizeof(ClusterType);
  ```
  (`ClusterFinderCUDA.hpp:442-445`)

  That is **120 004 B/frame at cap 3000** and **557 604 B/frame at cap 1700** — the
  4-byte count, aligned, then `cap` cluster slots whether or not they are used.

- **The host memcpy is occupancy-aware.** `materialize_slot()` reads the count and
  copies only that many:

  ```cpp
  uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
  n_found = std::min<uint32_t>(n_found, m_max_clusters_per_frame);
  if (n_found > 0)
      std::memcpy(results[frame_idx].data(), src, n_found * sizeof(ClusterType));
  ```
  (`ClusterFinderCUDA.hpp:138-158`)

  That is the 93 kB / 467 kB.

So: **93 kB is what the CPU copies, ~120 kB is what the PCIe link moves.** The deck's
payload table splits them for exactly this reason. Quoting 93 kB as "the D2H" would
understate it by ~23 % at 3×3 and ~16 % at 9×9.

The clamp on `n_found` is a bounds guard, not a tuning knob: the device counter is
incremented unconditionally and only the *write* is guarded, so the counter can read
past the cap on an overflowing frame.

### Q. Why is the cap 3000 at 3×3 but only 1700 at 9×9?

Both are lossless caps for their geometry — the smallest value at which no frame in
the campaign overflows. They differ because the cluster is 8.2× larger at 9×9, so the
same envelope buys far fewer slots, and because D2H cost scales with `cap × sizeof`:
raising 9×9 from 1500 to the lossless 1700 already moved D2H from 22.8 to 25.2 µs,
which on the `[f32]` arm overtakes the 23.9 µs kernel. Doubling to 3000 would put D2H
near 45 µs/frame and make the copy the binding engine (§8, §11.4).

### Q. Is the pinned buffer one frame, or one batch?

One **chunk**, and there are two of them.

```cpp
static constexpr int NUM_SLOTS = 2;
void grow_output_slot(int slot, size_t n_frames) {
    if (n_frames <= m_output_slot_capacity[slot]) return;      // only ever grows
    if (h_output_slots[slot]) CUDA_CHECK(cudaFreeHost(h_output_slots[slot]));
    CUDA_CHECK(cudaMallocHost(&h_output_slots[slot],
                              n_frames * m_output_bytes_per_frame));
    m_output_slot_capacity[slot] = n_frames;
}
```
(`ClusterFinderCUDA.hpp:1126-1137`)

So the pinned footprint is `NUM_SLOTS × chunk × m_output_bytes_per_frame`. At 3×3 with
a 2000-frame chunk that is `2 × 2000 × 120 004 B ≈ 480 MB`. The "only ever grows"
property matters: a run whose batches keep the same shape allocates **once**, and every
subsequent chunk reuses already-faulted pages.

### Q. What is a chunk, exactly, and what sets its size?

A chunk is the number of frames whose results fit in one slot — the pipelining unit.
`resolve_batch_chunk()` (`ClusterFinderCUDA.hpp:185-211`) picks it:

1. Aim for `DEFAULT_BATCH_CHUNKS = 8` chunks over the call.
2. Floor at `n_streams × 4` frames, or the per-chunk drain dominates.
3. **Cap by bytes, not frames**: `MAX_SLOT_BYTES = 128 MiB`.
4. Round up to a multiple of `n_streams`.

Step 3 is the one that surprises people. `128 MiB / 120 004 B` = **1120 frames at 3×3**;
`128 MiB / 557 604 B` = **240 frames at 9×9**. Without it,
`find_clusters_batched(whole_array)` scales the pinned allocation with the array —
20 000 frames at 9×9 gives 2500-frame chunks = 1.23 GB per slot, 2.46 GB pinned, whose
first-touch cost (~600 k page faults) lands inside the caller's timed region.

Step 4 is a **correctness** requirement, not tidiness: the device pedestal is
per-stream, so changing which stream a frame lands on would change the pedestal state
that frame is evaluated against.

`set_batch_chunk(n)` bypasses the whole thing, byte cap included.

### Q. What happens when a slot is full? Where do the results of frame 2241 go?

With chunk `C` and 2 slots, chunk 0 goes to slot 0, chunk 1 to slot 1, chunk 2 back to
slot 0. `m_next_slot = 1 - slot` (`:688`) is the whole rotation.

There is **no blocking wait** at the submit end — `submit_batch` *throws* if the slot it
is about to reuse is still in flight:

```cpp
if (m_slot_in_flight[slot])
    throw std::runtime_error("ClusterFinderCUDA: both batch slots are in flight "
                             "— call collect() before submitting a third batch");
```
(`ClusterFinderCUDA.hpp:676-680`)

The back-pressure lives in the **caller's loop order** instead. `find_clusters_batched`
submits chunk *b+1* and only then collects chunk *b*, so a slot is always free by the
time it comes round again:

```cpp
BatchToken nxt = submit_batch(frames.sub_view(b, e), first_frame + b);
drain(collect(tok));      // GPU runs chunk b while the host drains b-1
tok = nxt;
```
(`ClusterFinderCUDA.hpp:878-889`)

The waiting therefore happens inside `collect` → `finish_slot`, on
`cudaEventSynchronize`. That is what makes the steady-state period `max(GPU, host)`.

With a 2000-frame chunk, frame 2241 is in chunk 1 → slot 1, at byte offset
`241 × m_output_bytes_per_frame` into that slot's pinned block.

### Q. Does zero-copy (opt6) eliminate the D2H?

**No.** It eliminates the *host-side* copy — the `std::memcpy` in `materialize_slot()`
that moves data from the pinned buffer into freshly-`malloc`'d pageable memory, plus
the per-frame allocation that feeds it. The device→host transfer over PCIe is
untouched; the results still have to cross the bus.

```cpp
/// Nothing is copied and nothing is allocated: the returned view points into
/// the finder's pinned D2H buffer. The slot is held until the view is
/// released, so at most NUM_SLOTS - 1 further batches can be submitted while
/// it is alive. Consume, then release.
BatchView collect_view(BatchToken token);
```
(`ClusterFinderCUDA.hpp:815-833`)

The cost of the deal is the sentence at the end: holding a view holds a slot, so the
pipeline depth drops to 1 while you are reading. Consume promptly.

### Q. Do minor page faults still happen on the pinned buffer?

Yes, but they are paid **once per allocation**, not once per frame. `cudaMallocHost`
page-locks the region and its first touch is charged to whoever triggers it; because
`grow_output_slot` only ever grows, a steady-shape run touches those pages on the first
chunk and never again.

The faults that *scale with frame count* are on the other side — the per-frame `malloc`
inside `materialize_slot()`, which hands back fresh pages every frame. That is the
distinction that makes opt6 worth ×2.21 at 9×9: it deletes the faulting allocation, not
the transfer.

### Q. Where exactly is the host allocation, and where is its time paid?

Two allocations on this path, on completely different schedules. Conflating them is the
easiest way to misread the opt5/opt6 numbers.

**Pinned, once per shape** — `grow_output_slot()` (`:1126-1137`), one `cudaMallocHost`
per slot, guarded by `if (n_frames <= m_output_slot_capacity[slot]) return;`. Its cost
is a first-touch tax on page-locked memory, paid by whoever triggers it, and **it is not
paid again** while the batch shape holds.

**Pageable, once per frame** — this line, inside `materialize_slot()`:

```cpp
results[frame_idx].resize(n_found);          // <-- the per-frame allocation
std::memcpy(results[frame_idx].data(), src, n_found * sizeof(ClusterType));
```

Each `ClusterVector` allocates its own buffer, so every frame asks the allocator for a
fresh block — 467 kB at 9×9 — and then first-touches it. **That** is the term that
scales with frame count, and it is what makes the host loop *allocation*-bound rather
than bandwidth-bound. Its cost lands inside `collect()`, i.e. inside the caller's timed
region.

opt6 deletes the second one entirely: `collect_view()` hands back a view into the pinned
buffer, so there is nothing to allocate and nothing to copy.

### Q. Which copy are we actually measuring — the `memcpy` in `materialize_slot()`, or the Python-side `extend()`?

**The `std::memcpy` (and the `resize` that precedes it).** The Python-side accumulation

```python
clusters_cuda_per_frame.extend(
    cf_cuda.find_clusters_batched(data[start:stop], first_frame=start))
```

is *outside* what opt5-vs-opt6 compares. It appends already-constructed `ClusterVector`
objects to a Python list — pointer-shuffling and refcount work on objects that
`find_clusters_batched` has already returned. It moves no cluster payload.

The host term the deck talks about is entirely inside `find_clusters_batched`:

```
collect(tok)
  └─ finish_slot()        cudaEventSynchronize — waiting for the GPU, not copying
  └─ materialize_slot()   per frame: resize()  ← allocate + first touch
                                     memcpy()  ← the H2H copy
```

So when the slides say "~62 µs of malloc-and-copy per frame at 9×9", that is those two
lines, summed over the chunk and divided by frames. Keeping the accumulation out of the
timed region is deliberate; a benchmark that includes it is measuring the harness.

Worth stating plainly because it is the usual objection: the H2H copy is **not** a
device transfer and **not** a Python cost. It is one memcpy between two host buffers —
pinned to pageable — that exists only because the caller wants to own the memory.

---

## 1b · The kernel and the hardware

*(slides 7–10, 16)*

### Q. The tile holds pedestal-subtracted values. Why not stage the pedestal in shared memory too?

Because shared memory pays for **reuse**, and the pedestal has none at this granularity.

```cpp
sh[tid] = d_frame[gid] - d_pd_mean[gid];      // subtraction fused into the load
```

Count the reads per pixel:

| quantity | read by | reuse |
|---|---|--:|
| `frame - ped_mean` | every thread whose window covers it | **9×** (81× at 9×9) |
| `ped_rms` | only the centre thread, for its own threshold | 1× |
| `sum`, `sum2`, `mean` | only the owning thread, read-modify-write | 1× |

Only the first is shared by neighbours, and it is already the thing in the tile. Staging
`ped_rms` or the accumulators would add a global→shared→register hop for values touched
exactly once — pure cost. And the stencil never needs the pedestal itself, only the
difference, so there is nothing to reconstruct.

### Q. Would that change for a kernel that processed a batch of frames?

**Yes, and this is the interesting version of the question.** With `B` frames inside one
kernel, the same tile's pedestal would be read `B` times and the accumulators updated `B`
times, so reuse appears where there was none:

- stage `ped_mean` for the tile once, use it for all `B` frames — saves `B−1` global
  reads;
- accumulate `sum`/`sum2` **in shared memory** across the batch and write back once —
  saves `B−1` read-modify-write round trips on the arrays that opt7 showed are the
  kernel's dominant traffic.

That is a real optimisation targeting exactly the right thing. The catch is **semantic,
not technical**: the pedestal would then be frozen across `B` frames rather than one, so
every frame in the batch decides against a snapshot up to `B` frames stale. Annex A7
measures what a **one**-frame freeze costs (19 clusters in 23.2 M); a `B`-frame freeze is
a strictly larger perturbation and would need its own validation before it could ship.
Whether that is acceptable is a physics question about how fast the pedestal drifts, not
a performance one.

Worth noting the fallback if it is not acceptable: keep the per-frame semantics and stage
only `ped_mean` (read-only within a frame), which still saves the reads and changes
nothing.

### Q. Six blocks resident per SM at 3×3 — does the SM run all six at once?

No. **Resident is not issuing**, and the gap between the two is the whole point of
occupancy.

On sm_89 each SM has **4 warp schedulers**, and each issues **one instruction from one
warp per clock**. So at most 4 warps × 32 lanes = **128 lanes** are busy in any cycle,
against 128 FP32 cores. Six blocks of 256 threads is **48 warps resident** — the SM's
full complement (1 536 thread slots / 32 = 48 warp slots, hence 100 % occupancy at 3×3).

So the arithmetic is: 48 warps eligible, 4 issue, **44 waiting**. Those 44 are not idle
capacity going to waste — they *are* the latency-hiding mechanism. When a warp stalls on
a global load, the scheduler picks another already-resident warp in the same cycle, with
no context save or restore, because every resident warp owns its registers outright.

Three refinements worth having ready, because the block-level framing hides them:

- **Count warps, not blocks.** The four schedulers pick independently from their own
  partitions, so the 4 issuing warps may come from 4 *different* blocks — all of them
  resident on **this** SM, necessarily, since a scheduler can only choose among warps
  whose registers live in its own partition and blocks never span SMs. "Half a block
  runs per cycle" gets the arithmetic right (128 of 256 threads) and the mechanism
  slightly wrong.
- **A block never migrates.** Once resident, it stays on that SM until it retires, so
  the same SM does handle the next 128 pixels of that block. Other blocks go to other
  SMs — 625 blocks over 128 SMs.
- **This is what 33 % at 9×9 means.** Two blocks = 16 warps of the 48 slots. Only 16
  alternatives to switch to instead of 48. That is not a bug to fix: at 9×9 each thread
  does far more work per byte loaded, so 16 is enough to keep the pipes fed, and the
  register file is *exactly* full at two blocks (2 × 128 × 256 = 65 536).

### Q. Why only 77 % of theoretical PCIe bandwidth, even with pinned memory?

Pinning removes the *staging copy*; it cannot remove the protocol. The measured 13.2 µs
for 312.5 KiB = **24.2 GB/s against 31.5 GB/s theoretical**, and the missing 23 % is
mostly not recoverable:

- **The 31.5 GB/s is already the encoded line rate.** PCIe 4.0 ×16 is 16 GT/s × 16
  lanes, and 128b/130b encoding is already taken out. What remains on top is packet
  overhead, not encoding.
- **TLP framing.** Data moves as Transaction Layer Packets whose payload is capped by
  the negotiated Max Payload Size — commonly 256 B on desktop platforms. Each carries a
  header plus CRC, so ~6–10 % of the link is spent on framing before anything else. A
  machine negotiating 128 B pays roughly double that.
- **The reverse channel is not free.** ACK/NAK and flow-control credit DLLPs share the
  link.
- **312.5 KiB is a small transfer.** There is a fixed cost per copy — descriptor write,
  engine start, ramp — that a third of a megabyte does not amortise. `bandwidthTest`
  with multi-megabyte buffers reaches noticeably higher on the same link precisely
  because it does.

So 77 % on a 312 kB pinned transfer is close to what the link can actually deliver at
that size, and the deck calls it "true DMA speed" for that reason: the comparison that
matters is against **pageable staging at ~15 GB/s**, not against the datasheet.

Separately, the shipped pipeline reads **16.6 µs [s4]**, not 13.2 [s1] — +26 % from
H2D↔D2H contention. There is one copy engine per direction, but they share the link
(annex A1).

---

## 2 · Overlap, slots, and why more of them does not help

*(slides 17–18)*

### Q. If two slots leave the GPU idle, wouldn't three fix it?

No, and the argument is a standard queueing result rather than a measurement.

**With producer period G and consumer period H, an N-buffer pipeline has steady-state
period `max(G, H)` for every N ≥ 2.** Buffers *decouple* two stages; they do not speed
up either. Depth beyond 2 only helps when the periods **vary** — it absorbs jitter, at
the cost of latency and pinned memory. Here the work per chunk is near constant, so
there is no jitter to absorb.

The figure on slide 18 makes it visual: the host lane is byte-identical in the 2-slot
and 3-slot strips because it is already back-to-back. The third slot lets the GPU
front-load instead of stalling between chunks — the idle moves, it does not shrink —
and both strips cross the same finish line.

The concrete version usually lands better than the theorem: **a third slot needs
somebody to fill it, and the only host thread is inside `collect()`.** Adding slots
without adding a producer is adding storage to a queue that is not storage-bound.

### Q. Then would a producer thread help? The host end is serial, after all.

It was tried and reverted; the comment survives at `ClusterFinderCUDA.hpp:130-136`.

A producer thread would let submit and collect overlap, but the host term is dominated
by **malloc + first touch**, which is allocator-serialised anyway. Measured: **2.27 M
faults at 8 threads against 9.7 k at 1**, for a 6 % gain at best and a **33 % loss**
when results are freed promptly — extra threads get their own glibc arenas, which
destroys heap reuse between calls and costs more in page faults than the parallel copy
saves.

The comment ends with the right conclusion: *stop allocating per frame, do not copy
faster.* That is opt6, and it is worth ×2.21 where the thread pool was worth −33 %.

### Q. `submit_batch` is asynchronous but `collect` is synchronous. So why not thread the *collect* side, and then more slots would pay off?

**The premise is right, and so is the reasoning from it.** Confirmed in code:
`submit_batch` is enqueue-only in steady state — `cudaMemcpyAsync` H2D, kernel launch,
`cudaMemcpyAsync` D2H, `cudaEventRecord`, return. (Not on the *first* call, where
`sync_pedestal_to_device()` and `grow_output_slot`'s `cudaMallocHost` are both blocking.)
`collect` is the synchronous half: `cudaEventSynchronize`, then the `resize` + `memcpy`
loop.

And the double-buffering theorem does **not** forbid what you are proposing. It says
period = `max(G, H)` for every N ≥ 2 — more *slots* cannot help because slots do not
change `H`. Making the **consumer faster** changes `H`, which is a different lever
entirely. If `H` could be driven below `G`, the pipeline would become GPU-bound and sit
on the floor. Nothing in the theory rules that out.

**It was measured, and it lost.** The reverted experiment at `ClusterFinderCUDA.hpp:130-136`
*was* a parallel collect — a thread pool over the per-frame copy loop, exactly this idea:

> …the work is one 467 kB malloc + first-touch per frame at 9×9, so it is
> allocation-bound rather than bandwidth-bound. Extra threads get their own glibc
> arenas, which destroys heap reuse between calls and costs more in page faults than
> the parallel copy saves — **2.27 M faults at 8 threads vs 9.7 k at 1**, for a 6 %
> gain at best and a **33 % loss** when results are freed promptly.

The reason it fails is worth keeping, because it is not "threads are hard". `H` is
dominated by `malloc` + first touch, and glibc gives each thread its **own arena**. One
thread reuses the same freed blocks chunk after chunk and faults almost never; eight
threads each build a private heap and re-fault from scratch. Parallelising an
allocation-bound loop multiplies the very cost it is bound by. Changing the granularity
(a thread per *slot* rather than per frame) does not escape it — the same per-frame
`malloc`s happen either way.

So the answer is: **your lever is the right one, but threads are the wrong way to pull
it.** The way to make `H` smaller is to stop doing the work, not to do it in parallel —
`collect_view()` has essentially no `H` at all, and that is worth ×2.21 where the thread
pool was worth −33 %.

One consequence to note if you do adopt zero-copy: a held `BatchView` pins its slot, so
pipeline depth drops to `NUM_SLOTS - 1 = 1` while you read. Zero-copy trades buffer
depth for the copy — which is affordable precisely because there is no longer a long
host phase to overlap.

### Q. What were opt3's two barriers? The slide only shows one.

That gap is why slide 15 exists. opt3 removed **both**:

1. The per-round `cudaDeviceSynchronize`.
2. The **count-then-fetch round trip** — copy 4 bytes, block, read the count, copy N
   bytes, block. Two D2H transfers and two synchronisations per frame, replaced by one
   copy of a fixed envelope with no host involvement at all.

Barrier 2 is the reason the envelope is occupancy-blind in the first place: paying for
unused cluster slots is cheaper than asking the host how many were used.

---

## 3 · The 9×9 host term, and the 66.39 µs question

*(slides 18–19, §8.3, annex A4)*

### Q. The opt6 slide says the host copy is ~40 µs. Where does ~62 µs come from?

The 40 µs was **the memcpy alone, computed at bandwidth** — 467 kB at PCIe/DRAM rates.
It is a correct number for the wrong quantity: the host loop is **allocation-bound, not
bandwidth-bound**, so it undercounts the host term by roughly half.

### Q. Isn't 66.39 µs the end-to-end frame time, not the host time?

**Yes — and this correction is load-bearing.** An earlier draft argued: end-to-end is
`max(G, H)`, end-to-end measures 66.39, therefore H = 66.39, therefore the pipeline is
optimal. That is circular; it assumes the conclusion it then uses as evidence. 66.39 is
the end-to-end period of opt5 rep 1 and nothing more.

The host term is **not measured directly** — nobody timed the host loop in isolation.
It is inferred, and two independent routes agree on ~62 µs.

**Route 1 — fault correction.** opt5 at 9×9 is the one row in the ladder that never
reaches a fault-free plateau, because each rep meets a different allocator state for the
467 kB per-frame block:

| rep | µs/frame | minor faults | fault cost @ 0.68 µs | fault-free |
|--:|--:|--:|--:|--:|
| 0 | 78.56 | 460 283 | 15.65 | 62.91 |
| **1** | **66.39** | **151 601** | **5.15** | **61.23** |
| 2 | 67.34 | 95 884 | 3.26 | 64.08 |
| 3 | 81.26 | 506 241 | 17.21 | 64.04 |
| 4 | 73.97 | 334 303 | 11.37 | 62.60 |

The rate 0.68 µs/fault was fitted at **3×3** and applied here **out of sample**. It
collapses a **22 % raw spread into 4.6 %**, landing at 61–64 µs. A rate that had nothing
to do with the mechanism should not be able to do that.

**Route 2 — a rep that was already warm.** In the `[f32]` arm, rep 3 happened to run
with only **10 128 faults** (0.34 µs/frame) and measured **61.85 µs** with no correction
at all — inside the corrected `[f64]` band.

Contrast opt6 in the same file: **2 072, 0, 0, 0, 0** faults. And contrast opt5 at
**3×3**, which is clean — 30–96 k faults, 1.6 % spread. The contamination is specific to
one cell of the matrix, and its cause is exactly the allocation opt6 deletes.

### Q. Does the correction change the conclusion?

No. The host is the taller bar at 9×9 either way. What changes is the **margin**: it is
roughly **2× the GPU floor, not 1.3×**. Any figure drawing the 9×9 host cost at 40 µs
understates it by about half, which is why the opt5 strips and the result-path bars were
redrawn at 62.

### Q. How would you time that row honestly?

A fresh process per rep and a pre-touched result heap. Without both, the number that
comes out is an allocator state, not a throughput.

Related trap: freeing a ~10 GB result heap hands it back to the allocator and a
subsequent loop reuses it. On a cold heap the first loop pays the entire first-touch tax
and any printed ratio is meaningless. Both loops must be at plateau, and stale result
bindings must be dropped before timing.

---

## 4 · Pedestal timing — serial, frozen, and CUDA

*(slides 29–31, annex A7)*

### Q. What is the actual difference between `ClusterFinder` and `ClusterFinderFrozen`?

**Exactly one thing: when the pedestal is pushed.** The serial CPU finder updates the
pedestal *as the raster passes each pixel*, so a decision late in the frame is taken
against a pedestal that already contains this frame's earlier pixels. Frozen and CUDA
both decide against the **frame-start snapshot** and apply every update at the frame
boundary.

That makes the comparison factorable: `cpu vs frozen` isolates update **timing** with
the arithmetic held fixed, and `frozen vs cuda` isolates the **port** with the timing
held fixed. Comparing CUDA straight to the serial CPU confounds the two.

### Q. In the serial finder, can a pixel updated mid-frame ever affect a later decision?

Yes — through the **stencil**, never through the update itself. The distinction matters
and is easy to get backwards.

```cpp
void push_fast(const uint32_t row, const uint32_t col, const T val_) {
    SUM_TYPE val = static_cast<SUM_TYPE>(val_);
    m_sum(row, col)  += val - m_sum(row, col) / m_samples;
    m_sum2(row, col) += val * val - m_sum2(row, col) / m_samples;
    m_mean(row, col)  = m_sum(row, col) / m_samples;
}
```
(`Pedestal.hpp:215-222`)

`push_fast` touches **only the pixel's own accumulators** and never reads a neighbour.
So the update is **order-independent**: given the same *set* of updated pixels, serial
and frozen end a frame with a bit-identical pedestal. There is no accumulation
asymmetry to find.

The only channel for divergence is therefore a differing **decision** — and decisions
read the 3×3 stencil, which in raster order is half in the past (already possibly
pushed this frame) and half in the future (cannot be).

### Q. Which of the three tests is actually exposed to that?

All three can flip, but they are not equally exposed and they do not cost the same:

The 974 divergent pixels partition onto the three decisions with no remainder, and
the partition is the argument in one table:

| | reads | flips | clusters it explains |
|---|---|--:|--:|
| Test 1 | the stencil **max** | **619** | **0** |
| Test 3 | the stencil **sum** | 347 | 11 (all `frozen-only`) |
| local-max gate | two stencil values | 8 | 8 (all `cpu-only`) |
| | | **974** | **19** |

Test 3 collects the shift of *every* already-scanned neighbour — three above and one
left — where Test 1 feels at most the one that happens to be the argmax. That is why
the sum is the sensitive statistic.

Test 1 flips **most often and costs nothing**: a Test 1 flip moves a pixel between
`QUIET_UPDATE` and `SHADOW`, and neither of those stores. No cluster appears or
disappears; it only changes whether that pixel pushed, which feeds forward.

### Q. Can you prove Test 3 is the channel, rather than just argue it?

Two independent experiments, both in `python/tests/`.

**Instrumented** (`branch_trace.py` — both finders run their shipped logic, each records
a per-pixel branch code, the maps are diffed frame by frame). 974 pixels out of 1.6 × 10⁹
take a different branch, and the ones that change the cluster set decompose onto the
8/11 headline with **no remainder** (`cpu` finds 23 244 602 clusters, `frozen`
23 244 605):

```
frozen-only 11  =  QUIET_UPDATE -> TEST3_STORE       (all Test 3)
cpu-only     8  =  TEST3_STORE  -> TEST3_SKIP   (5)  (local-max gate)
                +  TEST1_STORE  -> SHADOW       (3)
```

**Ablated** (`AARE_TEST3_ENABLED = 0`, Test 3 compiled out of **both** finders):

| | Test3 ON | Test3 OFF |
|---|--:|--:|
| clusters (cpu) | 23 244 602 | 23 241 342 |
| divergent pixels | 974 | 584 |
| first divergence | frame 4 | frame 30 |
| frozen-only | 11 | **0** |
| cpu-only | 8 | 4 |

The 11 go to zero exactly.

### Q. Why does `cpu-only` go 8 → 4 rather than 8 → 8, if Test 3 is not involved in those?

Because ablating changes **which pixels push**. The pedestal then follows a different
path and the downstream ties are a different realisation. The *kind* is preserved — all
4 remaining are `TEST1_STORE → SHADOW`, the local-max gate — but the count is not, and
should not be expected to be.

### Q. So Test 1 is just downstream of Test 3?

**No — that was predicted and the measurement refuted it.** With Test 3 off, the first
divergence is still a Test 1 flip at frame 30, and *nothing diverged before it*, so the
two pedestals were provably identical when it happened. Test 1 initiates on its own. It
is simply ~7× slower to do so (frame 30 against frame 4) and **cannot create a cluster
by itself**.

### Q. Is the instrumentation in the shipped path?

No. `AARE_BRANCH_TRACE` defaults to **0** and every write folds away at compile time, so
the CPU baseline whose throughput the deck quotes is unaffected. `AARE_TEST3_ENABLED`
defaults to 1. Both live in `ClusterFinder.hpp` and `ClusterFinderFrozen.hpp`; both test
scripts document the rebuild. With tracing off, `branch_map` reads all-5 (`UNTOUCHED`).

---

## 5 · The three tests, and `c3`

*(slide 4, annex A7 · 1/2)*

### Q. Slide 4 says "the whole algorithm" and shows two tests. Is that all of them?

No — there are three, and A7 · 1/2 exists to say so. Slide 4's simplification is right
for the arc there (the point is the *three outcomes* shape: store, shadow, update), but
it is not complete.

```
v = frame[i] - ped_mean[i];   rms = ped_rms[i]
if (v < -nSigma*rms)              -> skip, no update
m     = max over the 3x3 window
total = sum over the 3x3 window
if (m > nSigma*rms)                    // TEST 1
    if (v == m) -> emit cluster        //   local-max gate
    else        -> shadow, no update
else if (total > c3*nSigma*rms)        // TEST 3
    if (v == m) -> emit cluster
else            -> update pedestal
```

The negative-value skip is a fourth branch but not a test in the same sense — it drops
pixels far *below* pedestal, which are detector artefacts rather than photons, and it
does not update either.

### Q. Where does `c3 = 3` come from? Is it tuned?

It is not tuned. It falls out of **variance addition**.

Test 1 asks whether one pixel is 5σ above **its own** noise. Test 3 asks the same
question of the **sum** of nine pixels — so all that is needed is the noise on that sum:

```
Var(v₁ + … + v₉) = Var(v₁) + … + Var(v₉) = 9σ²        (independent samples)
σ_sum            = √(9σ²) = 3σ
```

**Standard deviations do not add; variances do.** That is the entire content of the
square root: nine pixels give nine times the variance but only three times the rms, so
a threshold on the sum has to be 3× larger to carry the same 5σ meaning. Hence
`c3·nSigma·rms` is the *same criterion as Test 1, asked of the window instead of the
pixel*.

`c3 = sqrt(ClusterSizeX * ClusterSizeY)` in the constructor, so it generalises: at 9×9
it is `√81 = 9`.

### Q. Why bother summing at all?

Same algebra, read the other way: for a photon genuinely spread over the window, the
**signal adds linearly (×9) while the noise adds in quadrature (×3)** — a net √9 = 3×
gain in signal-to-noise. Test 3 catches a photon whose charge is shared out so widely
that no single pixel reaches 5σ, but the nine together do.

### Q. Doesn't that assume the noise is uncorrelated?

It does. What is being added is the **pedestal** noise, which is per-pixel readout noise
and uncorrelated between pixels to a good approximation. Correlated noise would need
covariance terms and would push `c3` **above** `√N`.

---

## 6 · Reading the annex A7 figure

### Q. Why does a photon look 3×4 pixels wide? Clusters are 3×3.

It doesn't — that is the shadow, and shadow is not the photon's 3×3.

> **Shadow is every pixel whose *own* 3×3 window contains something above 5σ.**

Crucially, **a shadow pixel need not be bright itself.** The condition is `m > 5σ` (the
*window's* max clears) and `v != m` (this pixel is not the peak). Three tiers, all
present at the site on the slide, against a 5σ bar of 85.5 ADU:

| ADU | branch | why |
|--:|---|---|
| 810.3 | **stored** | it *is* the window max |
| 345.0 | shadow | above the bar, but not the peak |
| 17.7 | shadow | nowhere near the bar — but its 3×3 reaches the 345 |

So charge shared across two adjacent pixels lights up the **union of every window that
can see either of them**, which is how a region reaches 3×4. In that 21×21 patch, 27
pixels clear the site's 85.5 ADU bar, 9 are local maxima, and 98 end up shadowed.

### Q. Why are the frozen numbers red rather than amber?

So that **amber means one thing in both panels**: a pixel that pushed the pedestal. On
the left it is the ~80 % sample class; on the right it is the ring around the four
already-scanned neighbours — and those four *are* pedestal samples, so the two uses
agree rather than collide. Frozen therefore takes red and serial keeps blue. Neither is
colour-alone: the frozen row is labelled and always sits above the serial row.

### Q. There's a lone shadow pixel below the site with amber on both sides. How?

Image pixel **(129, 245)**, four rows below the disputed pixel. It is the cleanest
illustration on the slide of something the algorithm does that is easy to forget.

Values (pedestal-subtracted), rows 128–130:

| | col 244 | col 245 | col 246 |
|---|--:|--:|--:|
| row 128 | 40.5 | −1.5 | −4.0 |
| **row 129** | **31.5** *(sample)* | **7.2** *(shadow)* | **−18.5** *(sample)* |
| row 130 | **85.2** | 2.6 | 11.2 |

Both (129, 244) and (129, 245) have windows that contain the 85.150 ADU pixel at
(130, 244) — it is one row down and one column left, so it sits in both. Their window
maxima are **identical: 85.150**. The right-hand neighbour (129, 246) has a window max
of only 16.958 and is a sample for uninteresting reasons.

So why do two pixels reading the *same* maximum take *different* branches?

**Because the 5σ bar is per-pixel.** The test is `m > nSigma * rms[centre]`, using the
noise of the pixel being tested, not a global constant. 85.150 lands right on the bar
(5 × 17.101 = 85.5 at the site), so a percent of rms variation decides it. From the
branch codes alone one can bound the two:

```
rms(129, 245)  <  17.030  ≤  rms(129, 244)
```

The left neighbour is *noisier*, so its bar is higher, so the same 85.150 fails to clear
it. Same evidence, different threshold, different answer.

Two things worth noticing while it is on screen. First, the 85.2 pixel at (130, 244) is
itself only a **sample** — it is its own window's max, but 85.2 does not clear its own
5σ bar and its window sums to 119.5, well under the ~256 Test 3 threshold. A pixel can
shadow a neighbour without being bright enough to be anything itself. Second, this is
the *same knife-edge* as the headline result: the disputed pixel differs by 0.09 ADU on
a threshold of 256.511. Near-threshold pixels are where every CPU/CPU disagreement
lives.

*(The dump carries no per-pixel rms map, which is why the bound above is a bound rather
than two printed numbers. `branch_site_dump.py` can be extended to record it.)*

### Q. Why is the left panel's marked pixel amber with a green ring, rather than green?

Because the panel is coloured by the **serial** finder, and serial *sampled the pedestal*
there — only frozen stored. Painting it plain green would read better and contradict the
Σ lines beside it.

### Q. Why do the two panels show different moments?

Deliberately, and each says so. The left is the **finished frame**: the branch map is
read once `find_clusters()` returns, so all 441 cells carry a final decision and the
arrows mean raster *order*, not progress. The right is **the instant the centre pixel
was tested**, which is the only moment at which the two models can be said to disagree —
four of its neighbours are in the raster's past and may already carry this frame's push,
four are in its future and cannot.

---

## Sources

| what | where |
|---|---|
| every quoted rate, fill, and fault count | `docs/ClusterFinderCUDA_benchmark_results.md` |
| the non-plateau row and the ~62 µs derivation | §8.3 |
| result-path code | `include/aare/ClusterFinderCUDA.hpp` |
| pedestal update | `include/aare/Pedestal.hpp:215` |
| the three tests | `include/aare/ClusterFinder.hpp:104-142` |
| branch decomposition | `python/tests/branch_trace.py` |
| the site dump behind the A7 figure | `python/tests/branch_site_dump.py` |
