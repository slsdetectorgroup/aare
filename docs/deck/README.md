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

## Layout guarantees, and how they are enforced

Two invariants are checked mechanically, because both fail silently otherwise.

**Nothing renders below 9 pt on the projected slide.** A figure's on-screen type size
is `raw_pt × (placement_width / figure_width)`, and neither factor is visible at the
point where the font size is written. `make_figs.py` closes that loop: `_placements()`
parses the placement width of every figure **out of the deck script itself**, so the
gate cannot drift from the layout it checks. Every run ends with either

```
legibility: every string in every figure renders at >= 9.0 pt on the slide.
```

or a list of offenders. Fix them; do not raise the floor. 9 pt on a 13.33 × 7.5 in
slide is about 1/60 of slide height, which is the conventional bound for readable
supporting detail at 6–7 m.

Note the feedback trap: `savefig(bbox_inches="tight")` grows the saved canvas to fit a
long in-figure caption, which shrinks the placement scale, which shrinks the caption.
Raising the font size can make text *smaller*. Shorten the string or re-lay the axes.

**No text runs past the footer line.** Convert and check:

```bash
libreoffice --headless --convert-to pdf --outdir /tmp/deck docs/cf_cuda_performance.pptx
python scratch/overflow.py /tmp/deck/cf_cuda_performance.pdf
```

Only page 1 may be flagged — that is the PSI template's own title slide. The same
script counts unrendered `**` markup, which is the usual symptom of putting markup in a
helper that does not parse it: `bullets`, `callout`, `table` and `code` understand
`**bold**`; `caption` does not, and nothing understands backticks or `*italics*`.

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
| `make_figs.py` | most figures, plus the legibility gate |
| `make_figs_kernel.py` | `fig_frame`, `fig_occupancy`, `fig_tile` |
| `QA.md` | questions the room asks, with the answers and where they are settled |
| `frame147.json`, `validation_tiers.json` | measured data two figures read |
| `branch_site.json` | the A7 site dump; written by `python/tests/branch_site_dump.py` |
| `CHANGELOG_2026-08-*.md` | dated records of past revisions; they keep the file names in use on those dates |
