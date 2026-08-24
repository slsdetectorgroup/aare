# 2026-08-24 — deck and report corrections

Session log for `docs/cf_cuda_fused.pptx` (via `docs/deck/build_fused_deck.py` +
`make_figs.py`) and `docs/ClusterFinderCUDA_benchmark_results.md`.
Deck rebuilt at **49 slides** (34 + 15-slide annex A1–A5).

---

## 1. "Floor" meant two things — resolved

- Root cause: `max(H2D, kernel, D2H)` and *best sustained rate* were both called the
  floor. They coincide at 3×3 (16.17) and diverge at 9×9 f64 (**32.66** profiled max vs
  **30.01** sustained), so the deck read as self-contradictory only at 9×9.
- Vocabulary unified on **"floor"** across ~9 slides: `% of peak` → `% of floor`,
  `Peak · the GPU floor` → `The GPU floor`, and three `Peak is the…` captions rewritten.
- **A1** now carries two separate rows — `engine max [s4]` **and**
  `FLOOR = lower of max, sustained` — so 32.66 and 30.01 both appear, each labelled.
- **12/34** (where the term is introduced) now says the max is a *profiled estimate* and
  the floor is the lower of it and the sustained rate.
- **25/34** caption defined the floor as the engine max while `fig_overhead` plotted
  30.01; caption corrected and now names which arm sets it at each cluster size.

## 2. Cap normalisation to 1700

- **19/34** and **34/34**: kernel claim `−40.7 %` → **`−40.5 %`** (39.86 → 23.70 µs,
  cap 1700 · s1).
- **A5·3** is now the only cap-1500 slide, flagged with ⚠ and the reason no re-run is
  needed (the RUNTIME column exists only in the trace; host-call cost is cap-independent).

## 3. s1 vs s4 — the 19/34 ↔ 22/34 confusion

- **Bug found**: `fig_f32_kernel`'s right panel was hardcoded to **cap-1500 s1**
  (`39.93 / 19.44 / 13.24`), contradicting the callout above it, and it showed the kernel
  binding in *both* arms — refuting the "D2H takes over" line on the same slide.
- Figure rebuilt as **two panels, s1 and s4**, same three engines, shared axis:

  | cap 1700 | kernel | D2H | H2D | binds |
  |---|--:|--:|--:|---|
  | s1 f64 | **39.86** | 21.97 | 13.20 | kernel |
  | s1 f32 | **23.70** | 21.95 | 13.22 | kernel |
  | s4 f64 | **32.66** | 25.25 | 20.77 | kernel |
  | s4 f32 | 23.94 | **25.24** | 20.54 | **D2H** |

- Binding engine named inline (`◀ binds`); dashed wall at 25.24; in-plot note that s4 is
  *union occupancy*, not duration (f64 kernel: 43.2 µs mean duration, 1.32× self-overlap
  → 32.66 µs/frame).
- **19/34** keeps the duration claim + payoff (`30.0 → 25.1 µs [s4]`); caption states the
  rule — *quote s1 for how long an engine takes, s4 for which one sets the floor*.
- **22/34** gets the kernel → D2H conclusion in a second amber callout, as the end of the
  Act III arc.
- Surfaced: the **s4** kernel gain is **−26.7 %**, not −40.5 % — self-overlap had already
  hidden part of the win.

## 4. Copy engines never overlap same-direction — timelines rescheduled

- `H2D_overlap` and `D2H_overlap` are **1.000 in every row of `probes.csv`** (one copy
  engine per direction). Only `kernel_overlap` exceeds 1.
- `fig_opt2_timeline` and `fig_streams` drew same-direction copies overlapping, which the
  hardware cannot do. **Both redrawn from a real schedule** (`_schedule()` in
  `make_figs.py`): H2D and D2H are single FIFO resources, kernels may overlap, a frame's
  stages stay ordered, and a stream waits on its own previous D2H.
- Bar proportions changed from an unsourced `12 : 22 : 12` to **3×3 `[f64 · s1]`,
  `13 : 15 : 5`** (13.14 / 14.72 / 5.31 µs) — these slides are 3×3-only. The H2D lane now
  fills solid *because H2D is the tallest bar at 3×3*, rather than that being asserted.
- opt2's barrier drain is no longer a drawn-in gap: it falls out of the schedule as the
  interval where the H2D engine starves waiting for the round to finish.
- Schedule reproduces the real step gain: opt2 18 units/frame → opt3 13, ratio **1.21×**
  against a measured ×1.18.
- **A false claim fell out**: the old opt2 figure said "all four in flight". With one copy
  engine the frame span (33) is only 2.5× the H2D stagger (13), so stream 0 has retired
  before stream 3 gets the engine. The figure now *computes* the max concurrency off the
  schedule and reports **3 frames in flight**.

## 4b. 32.66 vs 30.01 — two runs, two tools

- New §4 subsection *"The two columns are two different runs, made by two different
  tools"*, because this was the easiest thing in the document to misread.
- **32.66 µs/frame** = `probes.csv` `kernel_us_per_frame` = `kernel_busy_ms / 20 000`
  (653.140 ms of merged kernel intervals) — **one engine's union occupancy, under nsys**.
- **30.01 µs/frame** = `ladder_9x9.csv` opt8 `wall_s / 20 000` (0.60019 s) — the **whole
  pipeline end to end, unprofiled**. It does **not** come from nsys.
- Proof it cannot: the same profiled run reports `window_us_per_frame = 69.54 µs/frame`,
  2.3× slower than 30.01.
- `batch = 2000` is the chunk size *inside* each run (matched across harnesses so the
  overlap picture is comparable), never a denominator — both divide by `n_frames = 20 000`.
- Why the conflict resolves toward 30.01: under nsys submission is sparser, kernels
  overlap *less*, and a less-overlapped interval set has a *larger* union. The 1.32× is a
  lower bound on the real overlap.

## 5. `fig_cancellation` right panel redrawn

- Was: `axhline(42)` labelled "f32 error floor", damage shaded to rms 6.5 — and
  6.5² = 42.25, i.e. the line and the shading were each other's source. ~14× too large in
  variance, and inconsistent with the same figure's left panel (which draws 3).
- Now sourced from `pedestal_precision_f32_cancellation.md` §5–§6: floor **±3–4 ADU²**,
  axes rescaled to `(0, 6) × (0, 40)`, two zones (**rms < 2** clamped to 0; **rms 2–5**
  corrupted but not clamped), and the doc's three table rows marked on the curve.
- Now consistent with the slide's own bullet and with the rail's "~1–2 % of the sensor".

## 6. Layout fixes

- **19/34**: bottom-right callout ran to 7.38 and covered the page number (footer 7.14) →
  now 6.20 → 7.08.
- **13/34**: timeline picture overlapped its callout by 0.08" → figure 4.77 → 6.41,
  callout 6.48 → 7.08.
- **13/34** and **14/34** re-laid out for the taller annotated timeline figures.

---

## Report — `ClusterFinderCUDA_benchmark_results.md`

Audited against the CSVs in `python/tests/perf/results/`.

**Cap-1500 residues corrected**

- §0 `max_clusters_per_frame` 9×9: `1 500` → **`1 700`**, plus a paragraph on which
  sections deliberately keep 1500 (§12.1–§12.2, the page-fault diagnosis).
- §0 provenance table listed only the two `2026-08-18` dirs → now all five, including
  `2026-08-20_{f64,f32}_cap1700` and `_capAB`, which §4 and §6 already cite.
- §4 9×9 H2D interference: `13.24 → 19.74 µs (+49 %)` → **`13.22 → 20.54 (+55 %)`**, old
  reading kept as an explicit cap comparison.
- §6 / §11.2 / §16: opt4 at 9×9 `1.02×` → **`1.03×`** (3 places).
- §6 / §12.2: "a 24 % spread" → **`14 %`** (24 % was the cap-1500 spread).
- §6 / §16: "37 % of peak" → **`38 %`**.
- §11 / §15: opt7 kernel `−40.7 %` → **`−40.5 %`**.
- §11.4: kernel and D2H "6 % apart" → **`5 %`** (23.94 vs 25.24).

**Other corrections**

- §15 claimed the `.sqlite` files are *committed* — they are gitignored by design
  (`results/.gitignore`) and absent from the tree. Now says so; `probes.csv` named as the
  durable record.
- §15 said the deck is "29 slides" → **34 + 15-slide annex**; noted the `.pptx` is
  untracked and rebuilt from the script.
- §15 read "the same f64 9×9 **s4** run … ~39.9 µs per kernel instance" — 39.9 is the
  cap-1500 **s1** duration. For that s4 run the per-instance mean is **43.2 µs**
  (863.51 ms / 20 000). Corrected, with the point spelled out.
- §6: the CPU baseline row (665.22 µs) comes from `2026-08-19_cpu_threads/`, not the
  cap-1700 ladder (688.28 µs). Provenance footnote added.

**Additions**

- §4.1: new reading — only the kernel row ever overlaps; H2D/D2H stay true durations and
  *rise* under contention while the kernel *falls*.
- §4: note that the report's "peak" (FPS) and the deck's "floor" (µs/frame) are the same
  quantity in reciprocal units, and that neither is the raw engine max.

---

## Still open

- `fig_bottleneck` is generated by `make_figs.py` but referenced by no slide since 22/34
  moved to `fig_f32_absolute`. Dead weight, harmless.
- Slide 29/34's `[f64 ped]` validation rows are not backed by committed JSON.
