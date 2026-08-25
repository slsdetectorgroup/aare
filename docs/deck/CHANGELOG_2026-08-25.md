# 2026-08-25 — projection legibility, and the 30-minute restructure

Session log for `docs/cf_cuda_fused.pptx` (via `docs/deck/build_fused_deck.py` +
`make_figs.py`). Deck rebuilt at **51 slides**: title + hero + **33 numbered**
(3–33) + 7 dividers + **11 annex** (A1–A6).

---

## 1. One projection floor, enforced in two places

The back row is 6–7 m from the screen. On a 13.33 × 7.5 in slide, **9 pt is
about 1/60 of the slide height** — the conventional lower bound for supporting
detail. That is now the floor for *everything*, on the slide and inside every
PNG, and it is enforced by code rather than by hand.

**Text set in PowerPoint** — `run()` clamps to `MIN_PT = 9.0`. Enforced in the
helper, not at the ~400 call sites, because a floor applied by hand is a floor
one new caption silently drops through. Consequences: table headers 8 → 9,
statstrip labels 8 → 9, rail labels 8.5 → 9, code titles 7.5 → 9, page numbers
8.5 → 9, the 8.5 pt captions → 9.

`code()` had a **constant** line height of 0.148 in tuned for 8.5 pt, so raising
the type pushed the last line out of the panel. It now scales: `lh = 0.0174 ×
size`, which reproduces the old value at 8.5.

**Text inside figures** — the real problem, and it was invisible where the font
size is written. A matplotlib fontsize is in points of the *figure's* inches;
the PNG is then placed at some other width, so what the audience reads is

```
effective_pt = raw_pt × (placement_width_in / figure_width_in)
```

`make_figs.py`'s `save()` now takes the placement width, walks every `Text`
object in the figure after drawing it, and prints/collects anything below the
floor. `legibility_report()` at the end of the run is the gate. The placement
widths are **parsed out of `build_fused_deck.py`** rather than hardcoded, so the
check can never drift from the layout it is checking.

Measured before the fix:

| figure | PNG | placed | scale | smallest text |
|---|--:|--:|--:|--:|
| `fig_mismatch147` | 8.97 in | 9.40 | 1.05 | **4.8 pt** |
| `fig_opt2_timeline` | 10.64 in | 7.90 | 0.74 | **4.8 pt** |
| `fig_regpressure` | 10.71 in | 7.90 | 0.74 | **6.3 pt** |
| `fig_f32_kernel` | 8.00 in | 7.50 | 0.94 | **6.4 pt** |
| `fig_streams` | 6.36 in | 6.55 | 1.03 | **6.6 pt** |

13 of 22 placed figures failed. All 22 now pass.

Three causes, three fixes:

- **Self-inflating captions.** `savefig(bbox_inches="tight")` grew the saved
  width to fit a long in-figure caption, which shrank the scale factor, which
  shrank the caption further — the longer the text, the smaller it rendered.
  Four such strings (`fig_opt2_timeline`, `fig_streams`, `fig_f32_kernel`,
  `fig_graphs`) were provenance or verdict text, not data labels. They moved to
  slide captions and speaker notes, where a point is a point.
- **Declared width far above placement.** `fig_regpressure` 11.2 → 7.9 in,
  `fig_varfloor` 5.6 → 4.35 in, `fig_graphs` re-laid out at 7.6 in.
- **Unfittable text.** `fig_mismatch147` printed an ADU value in each of its
  masked-in cells at 4.6 pt. Dropped here, then **restored in §5** once the
  figure was given a wider placement — see there.

Then 75 individual sizes were raised — only those below their figure's floor, so
the internal hierarchy is preserved.

**Layout fallout, all fixed:** slides 9, 10, 20, 25, 33 and A5 were re-laid out
where the larger type pushed content into a figure or past the footer. Verified
with `pdftotext -bbox` across all 51 pages: no text below the footer line except
the PSI template's own title page.

---

## 2. Restructure for a 30-minute slot

### New: the measurement convention (12/33 here, **moved to 18/33 in §5**)

Promoted out of A1. Everything from slide 13 on is quoted as `[s1]` or `[s4]`
and compared against a "floor", and none of those three words was defined
anywhere the audience would see it. Three definition cards (s1 / s4 / floor) and
one figure; the numbers stay in A1.

`fig_measure` is new: four stream lanes from the **real** `_schedule()`, so the
H2D bars queue because there is one copy engine per direction (measured overlap
1.000 in every row of `probes.csv`) while kernels sit on top of each other in
time. The two union strips beneath are computed off that schedule. No
microseconds on that panel — it reproduces the *shape* of the 9×9 pipeline, not
its exact overlap factor, and measured numbers next to a schematic invite being
read across. The right panel is measured: 20.77 / 32.66 / 25.25 at s4, the
dashed profiled max, and the 30.01 sustained rate that is the actual floor.

### Reordered: 9 and 10 swapped

Registers (the cause) now precede occupancy (the effect). The old order taught a
metric and then said it was not the point. Slide 10 opens with occupancy in one
sentence — *when a warp stalls the SM switches to another resident warp;
occupancy is how many alternatives it has* — so the 33 % lands as a consequence
of the register budget, not as a failing grade.

### Merged: 20 + 21 → one slide

The correctness trap and the fix. The ULP arithmetic of the error floor is the
least transferable part of the story; what stays is *a small answer computed as
the difference of two large numbers is not computable in f32*, what it did to
the physics, and the two-line change.

`fig_cancellation`'s right panel is now the **deformed spectrum**. The f64 curve
is measured (`validation_tiers.json`, 23.2 M clusters). The f32 curve is
**reconstructed, and labelled as such on the slide**: its area is the measured
+28.06 % from §1 of the write-up, its shape is where §9 derives the excess (a
gate collapsed from ~225 ADU to ~0 admits the whole positive side, so the
population smears upward from zero). Nobody kept the broken build to re-run.

The old right panel — variance vs rms, the ±3–4 ADU² floor — is now
`fig_varfloor`, in A5.

### Cut to the annex

| was | now |
|---|---|
| 18 · two rejected routes (B′, B″) | **A2·3** |
| 21 · the variance rewrite | **A5** (+ `fig_varfloor`) |
| 26 · three ways a benchmark lies | **A6·1**, as it stood |

Slide 25 keeps only the artefact that moves a number the audience is shown —
first-touch page faults — plus one amber callout naming the other two (nsys
inflates host-side API calls ~4×, CUDA events measure a stream) and pointing at
A6. CUDA-event detail is not discussed in the arc at all.

Slide 19 (opt7) lost the s1-vs-s4 caption, which the measurement slide now owns.

### Detector context

MÖNCH03 specifications on slide 3: hybrid silicon, 400 × 400 at 25 µm pitch,
10 × 10 mm² active area, **1.3 kHz standard and 3–6 kHz with optimised readout**.
Slide 33 closes the loop: 58 495 FPS is **45× the standard rate and ~10× the
optimised ceiling**, with the two qualifications in the notes (frames are assumed
already in host RAM; at 9×9 the margin shrinks if the cap has to grow).

---

## 3. Renumbering

`N_SLIDES` 34 → **33**, `N_ANNEX` 5 → **6**. All `chrome()` indices reassigned
sequentially; six divider slide-lists and ranges rewritten; every cross-reference
retargeted (`expands slide N` on A2·3, A3, A4, A5, A6; `[engines: 19/33]`;
"slides 27–30"; the A2 part counts 1/2, 2/2 → 1/3, 2/3).

---

## 4. Verified

- `pdftotext -bbox`, all 51 pages: no text past the footer (PSI title page aside).
- No unrendered `**` markup anywhere.
- `legibility_report()`: every string in every figure ≥ 9.0 pt as placed.
- No stale cap-1500 values (`82.17`, `80.44`, `94.78`, "18 % SLOWER"); the one
  remaining "1 500" is the sentence explaining *why* the cap is 1 700.
- 20 em dashes on-slide, all pivots, appositions, or table "—" for empty.
- Visual pass over all 51 rendered pages.

## Still open

- `fig_bottleneck`, `fig_correctness` and `fig_variance_rewrite` are generated
  but placed on no slide. Harmless; `fig_variance_rewrite` joined the list when
  slide 21 moved to A5, where the code block says the same thing in less space.
- Timing: 40 slides in the arc. At 30 minutes that is ~45 s each, which is tight.
  Next cut candidates, in order: 14/15 (opt2 and opt3 could be one slide), 24
  (where the time went — the arc slides already carry it), 26 (first run — the
  point survives as a sentence on 25).

---

## 5. Second review pass (same day)

**Code-panel clearance, deck-wide.** The type floor grew every `code()` panel by
0.06–0.09 in, and eleven of them had less clearance than that to whatever was
drawn under. Found with a parser that computes each panel's height from its own
line count and size and compares against every later element with an overlapping
x-range, so this is not a slide-by-slide eyeball. All eleven now clear by
≥ 0.12 in; slides 8, 15 and 30 needed re-laying out around it.

**12/33 moved into Act III, now 18/33.** It talked about four streams (which
arrive at opt2) and 9×9 (which is not discussed until Act III), so it was
spending the audience's attention before either was earned. It now sits
immediately after the Act III divider, ahead of opt7 — whose two-panel figure is
the first place s1 and s4 are put side by side. Retitled "How the engine times
are measured".

Slide 12 (opt1) is where "% of floor" first appears, so it keeps a one-sentence
definition — *floor = max(H2D, kernel, D2H), the fastest a frame can go if the
host cost nothing* — and defers how each term is measured to 18. That callout
had been carrying the whole s1/s4/union argument; it is now four lines shorter.

**One colour code for the two builds in Act III: f32 = amber, f64 = blue.**
`fig_f32_absolute` and `fig_cancellation` already were; `fig_f32_kernel` had them
the other way round, so the same two builds swapped colour between slides 19 and
20. (The engine palette — H2D amber, kernel blue, D2H white — is a separate axis
and unchanged; both figures carry their own legend.)

**Titles.** 19 back to "FP32 halves pedestal traffic: −41 % kernel time".
20 → "Accumulate what is small, not what is large", with catastrophic
cancellation moved into the eyebrow; A5 renamed "The rewrite in full, and which
pixels the error reached" so the two no longer collide. The annex divider is
just "Annexes".

**29/33: the per-cell ADU values are back.** They were dropped because at a 9.4 in
placement a 25 × 25 grid gives each cell 13.5 pt of width, which cannot hold a
three-character number legibly. The figure is now placed at **10.6 in** — 15.3 pt
per cell — which fits the values at **9.0 pt as rendered**, so they are back
*and* they clear the projection floor. The centre marker moved behind the text
(it used to paint over the value in its own cell) and the slide's caption moved
to the notes to pay for the extra height.

**32/33: the knob that loses data is ringed.** New `frame_rect()` helper (an
outline, not a fill, so the row is not recoloured) and a new `RED` token used for
pointing only, never as a data colour. The bottom callout switched from amber to
the same red, so the ring and the warning read as one thing.

**Also:** `fig_f32_kernel`'s "39.86 ◀ binds" ran past its own x-limit onto the
right panel's tick labels (xlim 46 → 60, wspace 0.22 → 0.30); `fig_pinning`'s DMA
arrow label was wider than the gap it sat in.

Re-verified after every change: no text past the footer on any of the 51 pages,
no unrendered `**`, every figure string ≥ 9 pt as placed, and a visual pass over
all 51 rendered pages.

---

## 6. Third review pass (same day)

**19/33 · `fig_f32_kernel` label clearance.** A bar pair spans `y ± h`, so the
step percentage (`−40.5 %`, `−26.7 %`) at `−0.02 − h` sat exactly on the top edge
of the f32 kernel bar, and the verdict line at 5.5 % of axes height sat on the
H2D bars. Both now carry a full `h` of clearance beyond the bar pair, and the y
limits (`3.05, −0.98`) hold that margin explicitly rather than relying on default
padding — which the larger type had eaten. The legend moved to the one region no
label reaches, between the D2H and H2D rows, and the dashed 25.24 wall dropped
below the labels in z-order so it stops crossing the `23.94`.

**18/33 · the floor's units, spelled out.** The card read "1 / the busiest
engine" while the panel under it was labelled in µs and the annotation said
"FLOOR 30.01" — reciprocal in one place, microseconds in the other, with nothing
saying they are the same quantity. They are: the busiest engine's busy time per
frame is µs/frame, and one divided by it is FPS. **30.01 µs/frame is 33 323
FPS.**

- Card retitled "Set by the busiest engine", body now names both units.
- The figure annotation reads `FLOOR 30.01 µs/frame = 33 323 FPS`.
- The slide's closing callout says the deck quotes the floor both ways.
- The speaker notes gain a paragraph on when each unit is used: µs/frame when
  comparing engines to each other, FPS when comparing against the CPU baseline
  or the detector's frame rate. `% of floor` is the same ratio either way.

One consequence worth recording: the notes previously said "the floor is
1 / max(H2D, kernel, D2H) at s4 … 1 / 32.66 µs = 30 600 FPS", which mixed the two
forms in a single sentence and rounded loosely. It now reads "the max is 32.66 µs
= 30 618 FPS", with the reciprocal taken once and named.

Cards grew 1.52 → 1.70 in to hold the longer floor definition; `fig_measure`
re-placed at 10.3 in so the stack still clears the footer.

---

## 7. The CPU die, mirroring the GPU die (5/33)

Slide 6 had a die photograph with the compute units boxed; slide 5 had only a
block diagram, so the two machines were compared as schematics and never as
objects. Added `img_cpu_die.png` — Intel "Comet Lake" 10-core Core i9, same
CS149 source and the same crop recipe (`scratchpad/crops.py`: content box, light
card, rounded alpha edge).

**Kept whole rather than cropped to the ten cores.** The L3 slab on the right and
the I/O block on the left are close to half the die area, which is the callout's
own argument — *"the ALUs are the small part"* — standing next to it in silicon.

**On placement.** A strict mirror of slide 6 is not available: the AD102 die is
square (1.01:1) so it fits the narrow left slot, while the Comet Lake die is
2.35:1 and at any width where its core labels could be read it is 3.7 in tall.
Stacking it under the Skylake diagram at a shared width does not fit either —
both are landscape, and two of them plus the callout overruns the column by
~1.1 in. What did fit, without shrinking the Skylake diagram below the size at
which its `Fetch/Decode` and `ALU` labels survive projection:

| element | before | after |
|---|--:|--:|
| Skylake core diagram | 7.3 in | **6.5 in** |
| Comet Lake die | — | **4.55 in** |
| callout | 7.9 in, under the diagram | 3.18 in, beside the die |

Two bands: the core diagram across the top, then the die bottom-left with the
callout beside it. The diagram still dominates — 6.5 in against 4.55 — but the
die is now large enough that the ten boxed cores are countable at a glance and
the orange L3 slab is unmistakable, which is the whole reason it is there. At
6.5 in the diagram's `Fetch/Decode`, `ALU` and `L2 Data Cache` labels all still
project (checked by rendering it at 4.75, 5.6 and 6.5 in before choosing).

The die's own core numbering stays illegible, which is fine and matches slide 6:
neither die is read, both are counted. The caption carries the claim ("10 cores
boxed; nearly half the die is cache and I/O") exactly as slide 6's does ("144
blocks, 128 enabled on this card. One SM boxed.").

What moved to pay for it: the four-line Skylake-vs-Zen 4 caveat caption is now
speaker notes, along with a new paragraph on why ten cores do not fill the die.

**Also on 6/33** (pre-existing, same class): the closing caption sat at 6.90 and
its second line rode on the progress track. Trimmed to one line at 6.82, with the
FP64-scarcity remark moved into the notes — where it now says explicitly that the
V100 diagram *understates* it, since a GeForce part runs FP64 at 1/64, and that
this is half of why opt7 pays.

---

## §8 — Code panels read as objects

Twenty-one code panels were drawn on `CODEBG = #0E1420`, which is **1.03:1**
against the slide background: not a boundary, the same colour with extra steps.
They were found only because the text inside them was monospaced. On a projector,
whose black level is poor, they were nothing at all.

Lightening the fill cannot fix that, and the contrast table says why:

| code surface | vs. BG | vs. PANEL | ACCENT ink on it | MUTED ink on it |
|---|--:|--:|--:|--:|
| `#0E1420` (was) | 1.03 | 1.06 | 5.11 | 4.22 |
| `#121A28` (= PANEL) | 1.09 | 1.00 | 4.84 | 4.00 |
| `#17202E` (now) | 1.16 | 1.07 | 4.54 | 3.75 |
| `#243043` | 1.43 | 1.31 | 3.69 | 3.05 |

A fill lifted far enough to look wrong still reaches only 1.43:1, and it charges
for it in the two inks that live on that fill: the box gets more visible as its
highlighted tokens get less so. **A 1 pt line at `#3A4C66` gives 2.18:1 against
the background and costs the ink nothing**, because it never touches the ink's
ground.

So: framed, with the fill lifted only as far as it is free.

- `CODEBG` `#0E1420` → `#17202E`, a hair above `PANEL` rather than below it, so
  the panel has some body without eating the accent tokens.
- New `CODEEDGE` `#3A4C66`, 1 pt, on the rounded rect; corner radius pinned to
  `adjustments[0] = 0.055` so wide panels read as panels, not pills.
- New `CODEDIM` `#7C8A9E` for the `//` comments and the panel title, both of
  which were `MUTED` — tuned against the slide background, and down to 3.75:1 on
  the new fill. `CODEDIM` restores them to **4.67**, above where `MUTED` started.
  Used inside code panels only; `MUTED` is unchanged everywhere else.

All of it lands in the `code()` helper, so the twenty-one call sites are
untouched and uniform by construction. Geometry is unchanged — the border is
centred on the existing boundary, extending 0.007 in — and the overflow sweep is
clean across all 51 pages.

**What this buys, on 15/33.** The right column stacks three objects that were
previously two-and-a-half. They now read as three kinds without being read:

| object | container | means |
|---|---|---|
| code panel | framed, lighter fill, rounded | source |
| callout | flat panel, coloured left spine | conclusion |
| caption | none | provenance |
