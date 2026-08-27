# 2026-08-27 — 15 pt body, wider margins, and a layout checker that works

Session log for `docs/cf_cuda_performance.pptx` (via `docs/deck/build_performance_deck.py`
+ `make_figs.py` + `make_figs_kernel.py`). Deck rebuilt at **36 numbered slides**,
56 pages. The rendered PDF now audits **clean**: zero overlapping text, zero text
past the footer.

---

## 1. The content box widened to `[0.50, 12.90]`

It was `[0.70, 12.70]`. Narrowing the two outer margins buys 0.4 in of line
length — about 3 % more characters per line at every size, and on the widest
slides a whole line back.

It was applied as an **affine map** of x and w over every module-level layout
call, not by hand: nothing changes proportion, columns keep their relative gap,
and only the two margins give up space. `M`, `COL`, `RAIL_X` and `RAIL_W` moved
with it, along with the geometry inside `chrome()`, `annex_chrome()`, `rail()`
and `section()`, which the rewriter deliberately did not touch (inside a helper
every x is an offset from a caller-supplied origin).

Three module-level loops carry a **stride** rather than an offset — the hero
stat row, the s1/s4/floor cards, the closing four-card grid — and a stride is
not an x. Those were the only sites that needed a hand edit, and skipping them
would have overlapped the cards rather than moved them.

`deckgate.placements()` holds its own copy of the four tokens so it can evaluate
placement expressions. **It was still holding the old values**, so for one build
the legibility gate was checking widths no figure was placed at.

## 2. The interstitials read at body size

`section()`'s thesis went 13.5 → **16 pt** and its "coming up" list 12 → **15**.
The list is the reason the divider exists, so it is set at body size, not
caption size.

That costs height, so each entry is now **measured** rather than given a fixed
step: a two-line entry gets two lines of room instead of sitting on the one
below it. The pitch is `15 × 1.45 / 72`, not the `1.22` the nominal line spacing
suggests — see §6.

## 3. Slide-by-slide, what came off

Roughly 30 specific cuts. The pattern in all of them: the numbers and the
remarks stay, the sentence explaining why they are true moves to `notes()`.

- **3** — spectra caption cut to one clause; the closing line loses "not lit pixels".
- **4** — second bullet and the pedestal-timing rail note removed; the timing
  argument (and its link to A7) is now a speaker note.
- **5** — the Comet Lake die caption, which the picture already said.
- **6** — "A stalled warp is simply replaced, never reordered."
- **7** — right column shortened; the last callout had been sitting past the footer.
- **8** — the shared-memory-size paragraph → notes; the tile diagram grew into it.
- **9** — the register callout says the two cases and stops; rail note removed.
- **10, 12, 16, 18, 20, 21, 23, 24, 26, 28, 29, 30, 31, 35, 36** — one box, one
  note or one repetitive sentence each. Slide 12 lost its second callout entirely
  (the per-engine numbers moved to slide 13, where they belong to a measured run
  rather than to a diagram) and its DMA footnote.
- **21** — the s1 / s4 / floor cards are one-liners now, and the picture beneath
  them is 11.6 in wide instead of 10.6.
- **35** — the guidance column halved; the header row had been *under* the warning
  callout, not below it.

## 4. Two code panels had been silently deleted

Slides 33 and 34 each contained a bare list literal where a `code(...)` call
used to be — the `code(s, x, y, w,` prefix was gone and the list was evaluated
and discarded. Slide 33 had lost the frame-147 walkthrough, which **is** the
slide's argument, and 34 had lost "THE RECOMMENDED PATTERN", which is the API
the slide exists to teach. Both restored; slide 33's now also shows cuda's
`accept` branch, which had never been on the slide next to frozen's `reject`.

A third instance, on slide 15, had lost the opt2/opt3 barrier comparison.

## 5. The figures

Every figure was re-laid where text met text. The ones worth naming:

- **`fig_pinning`** was the worst: box labels wider than their boxes, arrow
  labels in a gutter narrower than the words in it. Boxes widened, arrow labels
  moved above the row.
- **`fig_tile`** — `set_aspect("equal")` shrinks the axes box to honour the
  aspect, so the data-space room reserved under the tile for its key was being
  squeezed and the key clipped. The key is drawn in **figure** coordinates now.
  The two panels also sit 0.06 apart instead of 0.135, and the canvas is 6.9 in
  rather than 7.6 — a narrower canvas at the same placement width is a *bigger*
  projected figure.
- **`fig_resultpath`** — the verdict is the second line of the axes **title**.
  As free-floating text it had nowhere to go: above the bars it met the value
  labels, below the axis it met two-line tick labels, beside them it met the
  gain. Bars also moved 1.35 apart, because the tick labels are ~0.95 x-units
  wide each.
- **`fig_streams`** — the barrier label was in the axes title's row. An axes
  title is anchored to the axes *box*, so it moves with the limits and not with
  the data; the fix is headroom in y. "time →" came inside the axes, because as
  an xlabel it sat on the sentence already written there.
- **`fig_arc`** — band labels shortened to `ACT I / II / III`. The gloss was
  wider than the one-bar ACT III band, so ACT III's label was on ACT II's.
- **`fig_pagefault`** — the key moved below the two rows; the right-hand panel
  owns that half of the figure and "first touch → fault" is 18 x-units wide,
  which was exactly the clearance left between them.
- **`fig_pedtiming`** — "every decision uses the frame-start snapshot" was at
  `y + 0.18`, sitting on the 2.2 pt trace it labels.
- **`fig_cancellation`**, **`fig_f32_kernel`**, **`fig_gpu_model`**,
  **`fig_regpressure`**, **`fig_occupancy`**, **`fig_overlap_9x9`**,
  **`fig_first_run`**, **`fig_measure`**, **`fig_mismatch147`** — same class of
  fix: headroom, or a label moved off the mark it was naming.

## 6. Line height is not what the nominal spacing says

Two constants, both wrong in the direction that clips:

- `code()` sets `lh = 0.0189 * size` in/line for Consolas. The old 0.0174
  under-counted by 8.6 %.
- LibreOffice sets a 15 pt UI line at about **0.31 in**, not the 0.26 that
  `line=1.25` implies. Every stack computed against 0.26 came out ~20 % short,
  which is one line on a five-line slide.

## 7. `deckgate.py`, and the kernel figures joining the gate

The projection floor, the placement table and the report moved into their own
module. There are **two** figure generators and only `make_figs.py` was ever
checked; `make_figs_kernel.py`'s three figures were consequently set at 7–8 pt
and nobody found out until they were projected. All font sizes there went up
×1.30 and the module now imports `deckgate`.

The floor itself is **10.0 pt**, two thirds of the 15 pt body.

A string may opt out with `gid="texture"` — for marks nobody is asked to READ,
where the pattern is the message and the digits are shading. Two figures use it:
`fig_mismatch147`'s per-pixel ADU values (a cell is ~0.18 in wide and a value can
be three digits, so nothing that clears the floor can also fit) and `fig_frame`'s
3×3 zoom. It is set per `Text` object, never per figure.

## 8. `audit_layout.py` now finds what it was built to find

Three changes, each of which had been hiding a real defect:

1. **Lines split at a column gutter.** Grouping words by baseline alone merges a
   rail row and a body bullet at the same height into one full-width "line".
   Two such lines overlap in x by construction, so the audit was reporting a
   collision on every two-column slide — 11 pages of noise that buried the four
   real ones.
2. **Overlap threshold 25 % → 10 %.** At 25 % a bullet ran a whole word under a
   code panel title without registering. It did, on slide 17.
3. **Two narrow exemptions**, so that a clean deck reports *clean* and the tool
   stays worth running: page 1 is the PSI template's own title slide, and a
   one-character "line" beside a real one is a superscript marker — which is what
   a footnote mark is.

Result: **0 collisions, 0 overruns**, from 34 pages with collisions at the start
of the 15 pt migration.

## 9. Wrap-join repairs

Eleven more glued words from earlier bulk rewrites — `over 128SMs`,
`thecount`, `opt1 andopt2`, `the hostadds`, `there,invisible`, `make thecolumns`,
`costsnothing`, `whosetrue variance`, `mmapthreshold`, `at tol= 0` — where two
adjacent Python string literals were joined with nothing between them. Python
concatenates them silently; only a render shows it.
