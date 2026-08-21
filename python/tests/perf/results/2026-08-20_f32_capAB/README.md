# 9×9 cap A/B — what `max_clusters_per_frame` actually buys and costs

**Build**: `[f32]`, `COMPUTE_TYPE = float`, `DEVICE_PED_TYPE = float`, git `7177f00`.
**Config**: 9×9, 20 000 frames, batch 2000, RTX 4090, idle GPU.
Both caps measured **on one build in one session, back to back**, which is the
whole point of this directory.

## Why this exists

The cap is not a safety bound. The D2H slot is

    output_bytes_per_frame = 4 + cap × sizeof(ClusterType)          (328 B at 9×9)

and `ClusterFinderCUDA` copies that slot **whole**, every frame, regardless of how
many clusters were actually found. So at 9×9 the cap sets the height of the D2H
bar directly. At 3×3 the cluster is 40 B and the same headroom is nearly free —
which is why this only ever mattered at 9×9.

The campaign ran 9×9 at **cap 1500**. Measured against the true per-frame
distribution over the same 20 000-frame block:

    mean 1422.1   std 25.0   min 1327   MAX 1633

so 1500 is **below the maximum**. It truncated 64 frames and discarded 2 715
clusters — 0.0095 %. The loss was silent: the kernel bumps its counter for every
detection and guards only the write (`if (write_idx >= max_clusters) return;`),
and the host then clamps the count, so a truncated frame is indistinguishable
from a short one. `ladder.py` now detects this and marks such rows `TRUNCATED`.

## The measurement

| cap | streams | kernel | H2D | D2H | binds | roofline | FPS |
|--:|--:|--:|--:|--:|---|--:|--:|
| 1500 | 4 | 24.11 | 19.64 | 22.77 | **kernel** | 24.11 µs | 41 480 |
| 1500 | 1 | 23.97 | 13.22 | 19.50 | kernel | 23.97 µs | 41 715 |
| 1700 | 4 | 23.94 | 20.54 | **25.24** | **D2H** | 25.24 µs | 39 614 |
| 1700 | 1 | 23.70 | 13.22 | 21.95 | kernel | 23.70 µs | 42 197 |

Sustained, unprofiled, same session (`ladder_9x9.csv` in
`../2026-08-20_f32_cap1700/`): opt8 reaches **39 775 FPS / 25.14 µs** at cap 1700,
against 42 274 FPS / 23.66 µs at cap 1500. So covering every cluster costs
**5.9 % of throughput**.

The kernel is unchanged across the two caps (24.11 vs 23.94 at s4, 0.7 % apart),
as it must be — the cap affects only the write guard. D2H scales with the slot:
at 1 stream, 492 000 B in 19.50 µs = 25.2 GB/s and 557 600 B in 21.95 µs =
25.4 GB/s. Linear, and bandwidth-bound.

## What it means

**The cap chooses which engine binds.** Same code, same data, one parameter:

- **cap 1500** — 0.0095 % of clusters discarded, **kernel-bound** at 24.11 µs.
  Act III's premise holds: the kernel is the tallest bar, and opt7's −40 % kernel
  translates into end-to-end gain.
- **cap 1700** — lossless, **D2H-bound** at 25.24 µs. opt7 still shortens the
  kernel by 40 %, but the frame no longer follows it: the result path is now the
  constraint.

Both are legitimate operating points and the choice belongs to the experiment,
not to the library. If 1 in 10 000 clusters is inside your statistical error —
and at 14 M clusters per 10 000 frames it usually is — cap 1500 is the faster,
kernel-bound configuration and the kernel-optimization argument is the right one.
If you need every cluster, take the 5.9 % and read D2H as the next target.

What is **not** legitimate is the state this campaign was in before today: a cap
believed to be non-truncating, silently discarding clusters, with a
kernel-bound conclusion resting on it and no way to notice.

## Artifacts

| file | what |
|---|---|
| `probes.csv` | the four rows above; `cap` is a column, so the A/B is machine-readable |
| `probe_9x9_{s4,s1_uncontended}_cap{1500,1700}.nsys-rep` / `.sqlite` | nsys traces. **The cap is in the filename** — two probes of one label at two caps are different measurements and must not overwrite each other |
| `env.json` | build identity. Trustworthy here: `common.assert_build_fresh()` ran first |

## Caveat on reading these

`roofline_fps` here is the **profiled** engine-occupancy estimate. Peak, as the
report defines it, is the *lower* of that estimate and the best rate the
unprofiled pipeline sustained. At cap 1700 the sustained 25.14 µs beats the
25.24 µs estimate, so peak is 25.14 µs / 39 775 FPS and opt8 sits **on** the D2H
floor.

An earlier attempt at this A/B on 2026-08-20 was discarded — see
`../2026-08-20_INVALID_stale_build/INVALID.md`. The tree had not been rebuilt
after the kernel header was edited, so an f64 kernel was measured and labelled
`float`. `common.assert_build_fresh()` was added in response and now guards both
`run_probes.py` and `run_ladder.py`.
