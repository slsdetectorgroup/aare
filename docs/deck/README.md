# The CUDA ClusterFinder deck

`docs/cf_cuda_performance.pptx` — kernel design, hardware limits, and the opt1→opt7
optimization ladder, told in three acts ordered by which bar is tallest.

36 numbered slides plus a 7-group annex, 56 pages once dividers and the title page
are counted.

## Build

```bash
python docs/deck/make_figs.py            # figures  -> docs/figures/*.png
python docs/deck/make_figs_kernel.py     # 3 more (fig_frame, fig_occupancy, fig_tile)
python docs/deck/build_performance_deck.py
python docs/deck/audit_layout.py <the rendered pdf>    # must print "clean"

# the shareable copy, regenerated from the .pptx after every rebuild
libreoffice --headless --convert-to pdf --outdir /tmp/pdfout \
    docs/cf_cuda_performance.pptx && mv /tmp/pdfout/cf_cuda_performance.pdf docs/
```

**Do not put emoji on a slide.** The colour-emoji planes (U+1F600 and up) are dropped
silently by the LibreOffice PDF export on this machine — the glyph renders as nothing at
all, with no warning. `U+263A ☺` and the other legacy BMP symbols live in DejaVu and
Segoe UI Symbol, render monochrome, and inherit the run colour. Use those.

**Two artefacts, deliberately.** `docs/cf_cuda_performance.pptx` is the presenter's
copy and carries every `notes()` block. `docs/cf_cuda_performance.pdf` is what gets
sent to an audience: LibreOffice's slide export drops speaker notes entirely, which is
checked — no notes-only string appears in the PDF text layer. Regenerate the PDF
whenever the deck is rebuilt, or the two drift.

Order matters: the deck embeds the PNGs, so regenerate figures first if you touched
either `make_figs*.py`. Running only the builder is fine when you have changed slide
text or layout alone.

Requires `python-pptx`, `matplotlib`, `pillow`, `lxml`. On this machine the only
interpreter with all four is `/home/ferjao_k/.conda/envs/py/bin/python` — the system
`python` is absent, `python3` is too old, and `python3.11` has no matplotlib.

`docs/cf_cuda_kernel.pptx` is an **input**, not an output: it donates the PSI theme and
the title slide, and every other slide of it is deleted at build time. Do not edit the
generated `.pptx` by hand — it is overwritten on every build. Edit the script.

## The type scale

One place, `build_performance_deck.py`, because 186 scattered `size=` arguments cannot
be reasoned about:

| token | pt | what it is for |
|---|---|---|
| `PT_BODY` | 15 | bullets: what the slide is claiming |
| `PT_LEAD` | 13 | callouts: the sentence to remember, already boxed and bold |
| `PT_RAIL` | 13 | rail row values |
| `PT_TABLE` | 11.5 | table cells |
| `PT_META` | 11 | captions and rail notes: provenance, deliberately quieter |
| `PT_CODE` | 10.5 | code panels — read by token, not word by word |
| `PT_LABEL` | 9.5 | small-caps labels on rails, tables, statstrips |

**Raising `PT_BODY` costs text.** 15 pt holds roughly 45 % of the characters 10.5 pt did
in the same box, because area scales with the square of point size. The scale is a
budget, not a preference: a slide that will not fit loses words, never type size.

Two line-height constants are calibrated against a LibreOffice render, not against the
nominal spacing, and both were wrong in the direction that clips:

- `code()` sets `lh = 0.0189 * size` in/line for Consolas. The old 0.0174 under-counted
  by 8.6 %, which is invisible on a short panel and eats the last line or two of a long
  one.
- LibreOffice sets a 15 pt UI line at about **0.31 in**, not the 0.26 that `line=1.25`
  implies. Budget 0.31 per body line when deciding what a slide can hold.

## Layout guarantees, and how they are enforced

Three invariants are checked mechanically, because all three fail silently otherwise.

**Nothing renders below 10 pt on the projected slide.** A figure's on-screen type size
is `raw_pt × (placement_width / figure_width)`, and neither factor is visible at the
point where the font size is written. `deckgate.py` closes that loop: `placements()`
parses the placement width of every figure **out of the deck script itself**, so the
gate cannot drift from the layout it checks. Both generators import it — `make_figs.py`
and `make_figs_kernel.py` — and each run ends with either

```
legibility: every string in every figure renders at >= 10.0 pt on the slide.
```

or a list of offenders. Fix them; do not raise the floor. 10 pt is two thirds of the
15 pt body, which is the usual lower bound for supporting type at 6–7 m.

The kernel figures were outside this gate for a long time and were set at 7–8 pt as a
result. If you add a third generator, import `deckgate` from it on day one.

A string may opt out with `gid="texture"`, and only for marks nobody is asked to READ —
a value printed into every cell of a pixel map, where the pattern is the message and
the digits are shading. Two figures use it: `fig_mismatch147`'s per-pixel ADU values
and `fig_frame`'s 3×3 zoom. Set it on the `Text` object, never on a figure.

Note the feedback trap: `savefig(bbox_inches="tight")` grows the saved canvas to fit a
long in-figure caption, which shrinks the placement scale, which shrinks the caption.
Raising the font size can make text *smaller*. Shorten the string or re-lay the axes.
The corollary is useful: a NARROWER `figsize` at the same placement width is a bigger
projected figure and bigger projected type.

**No text overlaps other text, and none runs past the footer.** Both are checked by
`audit_layout.py`, which reads the rendered PDF's word boxes:

```bash
libreoffice --headless --convert-to pdf --outdir /tmp/deck docs/cf_cuda_performance.pptx
python docs/deck/audit_layout.py /tmp/deck/cf_cuda_performance.pdf   # -> "clean"
```

Three things about it are load-bearing:

- It groups words into lines by baseline and then **splits each line at a column
  gutter**. Without that split a rail row and a body bullet at the same height merge
  into one full-width "line", and every two-column slide in the deck reports a
  collision.
- It asserts on the parsed word count. A broken bbox parser yields empty boxes and
  therefore a *clean* report, which is the worst failure mode a checker can have.
- Two exemptions are deliberate and narrow: page 1 is the PSI template's own title
  slide, and a one-character "line" beside a real one is a superscript marker.

Overlap has to cover 10 % of the narrower line to register. At 25 % a bullet could run a
whole word under a code-panel title without being reported, which it did.

## Numbering

Slide indices are explicit — `chrome(s, 18, …)` — and so are the section ranges and the
prose cross-references ("expands slide 28"). **Inserting a slide shifts all three.** As
of this writing that is 47 `chrome()` calls, 7 `section(… rng=…)` ranges with their item
lists (integer tuples in Act I–III, `"25–26"`-style strings after), and 28 prose
references. Renumber all of them in one pass and rebuild; the
progress track and the `N / 35` counter both read `N_SLIDES`.

**Optimisation slides carry an `OPT<n>` badge**, drawn by `chrome(… opt=n)` in the same
corner and the same way `annex_chrome` draws `A<n>`. The badge names the *rung*, not the
slide, so opt3, opt5 and opt7 repeat theirs across two slides each. When a slide has a
badge its eyebrow must not also say "optN" — that reads twice.

**The content box is `[0.50, 12.90]` on a 13.333 in slide.** It used to be
`[0.70, 12.70]`; narrowing the two outer margins bought 0.4 in of line length, which is
about 3 % more characters per line at every size. The tokens that carry it are `M`,
`COL`, `RAIL_X` and `RAIL_W`, and **`deckgate.placements()` holds copies of all four**
so it can evaluate placement expressions. Change one and change the other, or the
legibility gate starts checking widths no figure is placed at.

**Nothing on a slide may point outside the deck.** No notebook names, no
`python/tests/…`, no result directories, no "see the write-up" — the slides get shared
on their own, and a pointer to something the reader does not have is worse than no
pointer. Library header names (`ClusterFinderCUDA.hpp`, `clusterfinder_kernel.cuh`) stay:
they are the subject being presented, not a reference to somewhere else. Everything
removed lives in `notes()`; slide 36's notes carry the full index.

**Code panels have two highlight markers**, and they mean different things:
`«…»` renders ACCENT for whatever the slide is arguing about (a knob, a step number),
`‹…›` renders PALE bold and is reserved for the API surface — the call a user will
actually type. Keeping them distinct is why slide 34 can key its prose to numbered steps
without the method names disappearing into the same blue.

## Where the numbers come from

`docs/ClusterFinderCUDA_benchmark_results.md`, quotable rows only. Two conventions the
deck depends on, both defined in slide 21:

- **s1** is one stream — true, exclusive engine durations.
- **s4** is the shipped four-stream pipeline — engine *occupancy*, the union of
  intervals per frame. The kernel overlaps itself across streams (9×9 f64 reads 32.66 µs
  at s4 against ~43.2 µs per kernel); H2D and D2H do not, because there is one copy
  engine per direction.
- **host DRAM** on `pc-moench-04` measures **~71 GB/s** (threaded copy over a 1 GB
  array, 24 B/element: 8 read, 8 write, 8 read-for-ownership). Quoted on slide 12 so the
  wire's 31.5 GB/s can be compared against something real at both ends — it is 32× below
  VRAM but only ~2.3× below DRAM, and the slide says the narrower thing.
- **floor** = `1 / max(H2D, kernel, D2H)` at s4, taking the lower of the nsys estimate
  and the best rate actually sustained. One quantity, two units: 30.01 µs/frame =
  33 323 FPS.

One row is not at steady state and is flagged as such on its slide and in §8.3 of the
report: opt5 at 9×9, whose per-frame allocation never lets the fault count converge.

## Files

| file | role |
|---|---|
| `build_performance_deck.py` | the deck: tokens, helpers, every slide |
| `make_figs.py` | most figures |
| `make_figs_kernel.py` | `fig_frame`, `fig_occupancy`, `fig_tile` |
| `deckgate.py` | the projection floor and the placement table, shared by both generators |
| `audit_layout.py` | reads the rendered PDF: overlapping text, and text past the footer |
| `QA.md` | questions the room asks, with the answers and where they are settled |
| `frame147.json`, `validation_tiers.json` | measured data two figures read |
| `branch_site.json` | the A7 site dump; written by `python/tests/branch_site_dump.py` |
| `CHANGELOG_2026-08-*.md` | dated records of past revisions; they keep the file names in use on those dates |
