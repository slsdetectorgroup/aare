# ClusterFinderCUDA measurement campaign

Everything that produces a number in
[`docs/ClusterFinderCUDA_benchmark_results.md`](../../../docs/ClusterFinderCUDA_benchmark_results.md)
lives here, and everything it produces lands in `results/<date>_<build>/` with a
manifest, so any figure in the report or the deck can be traced back to the run
and the build that made it.

## Scripts

| script | produces | answers |
|---|---|---|
| `run_campaign.sh` | both arms | the whole thing: f32 ladder+probes → rebuild → f64 ladder+probes → rebuild back |
| `run_ladder.py` | `ladder_<dim>x<dim>.csv` | end-to-end wall / FPS / faults / counts for every step |
| `run_probes.py` | `probes.csv` + `.nsys-rep`/`.sqlite` | per-engine GPU times, duty cycles, **the rooflines** |
| `ladder.py` | — | the ladder *as a config matrix*; imported, not run |
| `common.py` | — | dataset, pedestal, fault bracketing, env capture, CSV format |
| `nsys_kernel_probe.py` | — | the workload `run_probes.py` traces; runnable alone under nsys |
| `gpu_span.py` | a duty-cycle table | *engine busy* vs *engine idle*; `analyze()` is importable |

## Running it

```bash
./run_campaign.sh              # both arms, ~1.5 h, unattended
./run_campaign.sh f32          # one arm

python run_ladder.py --dry-run             # 2000 frames, 1 rep — proves it executes
python run_ladder.py                       # 3x3 @ 100k, 9x9 @ 20k, 5 reps
python run_ladder.py --dims 9 --steps opt7 opt8    # harness labels — see the map below
python run_ladder.py --retain              # keep every ClusterVector (notebook behaviour)

python run_probes.py                       # 4 configs at 20k frames
python run_probes.py --only 9x9_s4
python gpu_span.py <rep>.sqlite 20000      # re-read duty cycles from an existing profile

# per-operation durations from a committed profile — no GPU, no rerun.
# Pass the .sqlite: the reps are newer than their exports, so the .nsys-rep path
# demands --force-export=true and rewrites the export gpu_span.py reads.
nsys stats --report cuda_gpu_kern_sum --report cuda_gpu_mem_time_sum \
    --format table results/2026-08-18_f64/probe_9x9_s1_uncontended.sqlite
```

`nsys stats` gives durations, not duty cycles — it cannot tell "engine busy" from
"engine idle waiting". Use it for *how long is one kernel* (`s1`); use `gpu_span.py`
for overlap, duty and the roofline.

**The GPU must be idle.** Both drivers abort above 5 % utilisation: a competing
process leaves per-operation averages intact while destroying the duty cycle and
the wall clock, so the failure is silent if you don't check. Close any notebook
first. `--allow-busy-gpu` exists but the numbers are not quotable.

## Step labels

The `--steps` flag and the `step` column of every CSV use the harness's internal labels,
which predate the report's numbering. The report carries the same map in its §15.

| harness label | step in the report | act |
|---|---|---|
| `cpu` | baseline | — |
| `opt1` `opt2` `opt3` `opt4` | opt1 opt2 opt3 opt4 | **I** — feeding the GPU |
| `opt5` | **route A** — CUDA Graphs, *rejected* | (fork after opt4) |
| `opt7` | **opt5** — chunked host↔GPU overlap | **II** — getting results back |
| `opt8` | **opt6** — zero-copy `collect_view()` | **II** |
| *(build axis, not a row)* | **opt7** — f32 device pedestal | **III** — the kernel |

Renaming the labels would rewrite recorded data, so they stay as they are; translate on
the way out.

## Fixed parameters

| | 3×3 | 9×9 |
|---|--:|--:|
| `N` | 100 000 | 20 000 |
| `max_clusters_per_frame` | 3 000 | 1 500 |
| `n_streams` | 4 | 4 |
| `BATCH_SIZE` | 2 000 | 2 000 |
| pedestal frames / `n_sigma` | 1 000 / 5 | 1 000 / 5 |
| reps | 5 | 5 |
| nsys probe frames | 20 000 | 20 000 |

Optimising over these is a separate exercise; what matters here is that every
step sees the same ones. Two are deliberate departures from earlier campaigns:
`n_streams` was 8 at 9×9 (8 streams buy no kernel concurrency there — instance
time +1 % — while inflating the event timer 3.5×), and probes were 2 000 frames
(too short for the clocks to ramp, which is how a 26.7 µs roofline was published
for a pipeline that sustains 24.25).

9×9 is held at N=20 000 because its result heap is ~5× larger per frame
(1422 × 328 B = 466 kB vs 2330 × 40 B = 93 kB); 100 k would need 46.6 GB to
retain against 98 GB free with no swap.

## Reading the output

**`cold` = rep 0 in a fresh process. `warm` = best of the remaining reps.**
Not the last rep — `collect()` does not converge, it oscillates between allocator
states. Measured at 9×9, opt4, one run: 85.8 / 73.7 / 86.6 µs with faults
520 k / 127 k / 519 k. Quoting the last rep there reports 86.6 when 73.7 was
achieved in the same run; the choice of rep would be doing the work, not the code.

**The `spread` column is a result, not noise.** Paths that allocate per frame
vary 3–28 % run to run; `collect_view()`, which allocates nothing, is
reproducible to 0.0–0.2 %. That contrast is the strongest argument for opt6
(harness `opt8`) — it is the only path whose throughput is *reproducible*.

**Each step runs in its own process.** The heap is process-wide, so in a shared
process every step inherits what the previous ones grew: `opt3` reports **2**
faults after opt1/opt2 have run and **92 251** on its own. `--no-isolate` restores
the fast path and makes the fault columns meaningless; throughput is unaffected
either way.

**Reps share a finder within a process** — a new one would reset the heap. The
device pedestal advances by `n_frames` each pass, so `n_clusters` drifts ~0.002 %
between reps. Compare steps at the same rep index, never across reps.

**opt7 is not a row.** It is `DEVICE_PED_TYPE` in
`include/aare/clusterfinder_kernel.cuh`. Run the matrix once per build and compare
directories; `env.json` records which arm you are in. The f64 arm is the report's
Acts I–II, the f32 arm is Act III.

**opt1/opt2 are 3×3 only** — `ClusterFinderCUDAOpt2` is registered for 3×3 in
`cuda_bindings.cu`, so the 9×9 ladder starts at opt3. They are also unaffected by
the opt7 flip by construction: their binding pins `PEDESTAL_TYPE` to `double`.
If they move between arms, something is wrong.

**The CPU baseline is first-pass only** and forced to 1 rep: `ClusterFinderMT`
cannot restart after `stop()`. Both speedup columns therefore divide by a *cold*
CPU number, which reads ~9 % generous.

**At 4 streams the probe's `kernel_us_per_frame` is engine occupancy**, the union
of kernel intervals over frames — not per-kernel duration. That is why f64 9×9
reads 32.08 µs at `s4` while each kernel is ~39.9 µs. Use `s1` for exclusive
kernel times (the opt7 claim), `s4` for "% of roofline".

**Expect zero-copy to land 2–3 % *under* its roofline.** The roofline is measured
under CUPTI, which dilates GPU op durations slightly, so it is a mild
over-estimate. "≥100 % of roofline" means *at the floor, within the profiler's
own systematic error* — not a measurement error. Take percentages from the **f32**
arm: the f64 9×9 `s4` kernel column is an interval union at `overlap = 1.36`, which
CUPTI inflates further, and zero-copy reads 6.4 % under it there.

## Two policies enforced in code

1. **Never warm up by processing frames.** The kernel pushes a pedestal update
   per pixel per frame, so a finder that has seen extra frames is no longer
   comparable with one that has not. Slots are pre-pinned with
   `reserve_output_slots()`, which allocates without transferring or launching —
   verified to leave cluster counts bit-identical.
2. **`time_kernels=False` everywhere**, including `ClusterFinderCUDAOpt2`, which
   gained the flag for this reason. With events on for one finder and off for
   another, the instrumented one pays a per-frame tax the other does not and the
   step between them absorbs it. Kernel times come from nsys, which is the only
   source correct under multi-stream load anyway.

## Results directories

```
results/<date>_<f32|f64>[_tag]/
    env.json         build, git rev, driver, GPU, DEVICE_PED_TYPE, timestamp
    manifest.csv     artifact -> config -> build -> which report section cites it
    ladder_3x3.csv   one row per (step, rep) — every rep kept, nothing averaged
    ladder_9x9.csv
    probes.csv       per-engine us/frame, duty %, overlap, bottleneck, roofline
    probe_*.nsys-rep openable in nsys-ui
    probe_*.sqlite   input to gpu_span.py
```

Current campaign: **`2026-08-18_f32/`** and **`2026-08-18_f64/`**.
`2026-08-12_f32_legacy/` is retired — see its `SUPERSEDED.md`.

> **Known wart:** `results_dir()` stamps *today's* date, so a campaign that spans
> midnight splits across two directories. That happened once already (the f32
> ladder and its probes landed a day apart) and had to be merged by hand, with a
> note added to `env.json`. Prefer a campaign tag over a date if this recurs.
