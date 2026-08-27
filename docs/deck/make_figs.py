"""Figures for docs/cf_cuda_performance.pptx — deck palette, dark.

Every number here is a quotable row from docs/ClusterFinderCUDA_benchmark_results.md,
i.e. from python/tests/perf/results/. Acts I and II are the f64 arm, Act III is
the f32 arm. 3x3 comes from 2026-08-18_{f32,f64}/; 9x9 was RE-TAKEN on
2026-08-20_{f32,f64}_cap1700/ because the campaign's 9x9 cap of 1500 sat below the
per-frame maximum (1633) and silently truncated 0.0095 % of clusters. All 9x9
numbers below are cap 1700, lossless.

CPU baseline = the BEST thread count, one per cluster size, from
python/tests/perf/results/2026-08-19_cpu_threads/. The campaign originally used
n_threads=48 on a 16-core / 32-thread Ryzen 9 7950X -- 1.5x oversubscribed, and
slower than the CPU can actually go. Every speedup in the deck divides by these:

    threads      3x3 FPS      9x9 FPS
        8         3 805          737
       16         6 594        1 237
       24     >> 6 762 <<      1 348
       32         5 942     >> 1 503 <<
       48         5 121        1 338      <- the campaign's original

So 3x3 divides by 6 762 (147.9 us/fr) and 9x9 by 1 503 (665.2 us/fr). The optima
differ because ClusterCollector's drain -- inside the timed region, as in
ladder.py -- scales with thread count while 9x9 clusters are 9x larger. There is
no per-arm CPU baseline any more: the CPU finder never touches DEVICE_PED_TYPE,
so the old f64/f32 split (201.16 / 191.26 us) only encoded run-to-run noise.

Ladder, warm, us/frame          3x3          9x9 (cap 1700)
    CPU MT (best)                147.88      665.22
    opt1                         63.26      --
    opt2                         40.44      --
    opt3                         34.26       82.44
    opt4                         25.98       79.83
    route A (graphs)             25.16       90.32
    opt5  chunked overlap        19.84       66.39
    opt6  zero-copy              17.10       30.01
    opt7  = opt6 on f32          16.31       25.14

The GPU FLOOR (the deck says "floor" everywhere; older drafts said "peak") is
1 / max(H2D, kernel, D2H), each term being that engine's BUSY
TIME PER FRAME (the union of its intervals) at the ladder's 4 streams -- and taken
as the LOWER of two estimates: the profiled engine occupancy, and the best rate the
unprofiled pipeline sustained. A sustained rate is an existence proof; the probe is
an estimate made in a loop nsys slows to ~69 us/frame, where kernels overlap less
and the union per frame reads high.

                 probe (nsys)      best sustained      FLOOR             binds
    3x3 f64      16.17 us          17.10 us            16.17 us -> 61 859  H2D
    3x3 f32      16.63 us          16.31 us            16.31 us -> 61 312  H2D
    9x9 f64      32.66 us          30.01 us            30.01 us -> 33 323  KERNEL
    9x9 f32      25.24 us          25.14 us            25.14 us -> 39 775  D2H

The last row is the point of Act III. At 9x9 cap 1700 the D2H slot is 544.5 KiB
and costs 25.2 us on BOTH arms. Under the f64 kernel (32.66 us) that is invisible.
opt7 cuts the kernel 40 % to 23.94 us -- below the D2H bar -- so the f32 floor is
D2H, not the kernel. Optimizing the kernel in bottleneck order ended by handing
the constraint to the result path, which is what success looks like.
"""
import sys
import json
import re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.text
import numpy as np
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import deckgate  # noqa: E402  -- the projection floor, shared with make_figs_kernel.py

OUT = Path(__file__).resolve().parent.parent / "figures"
OUT.mkdir(exist_ok=True)

BG     = "#0B1018"
PANEL  = "#121A28"
RULE   = "#1E2836"
ACCENT = "#1E90C2"   # Act I  / data 1
AMBER  = "#E8B25C"   # Act III / data 2 / warnings
PALE   = "#E7EDF4"   # Act II / data 3 / primary text
TEXT2  = "#A5B2C4"
MUTED  = "#6B7A90"   # non-data only: grid, axes, annotation
GREEN  = "#5CC8A0"   # rooflines / floors
RED    = "#E8695C"   # fig_test3 only: the frozen finder, so AMBER can keep its
                     # one meaning across both panels (a pixel that pushed the
                     # pedestal). Never colour-alone — the frozen row is always
                     # labelled and always sits above the serial row.

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 9,
    "text.color": PALE, "axes.labelcolor": TEXT2,
    "xtick.color": TEXT2, "ytick.color": TEXT2,
    "axes.edgecolor": RULE, "axes.facecolor": "none",
    "figure.facecolor": BG, "savefig.facecolor": BG,
    "axes.grid": False, "svg.fonttype": "none",
})


DPI = 220

# ------------------------------------------------------------ legibility gate
# A matplotlib fontsize is in points of the FIGURE's own inches. The PNG is then
# placed on the slide at some other width, so what the audience actually reads is
#
#     effective_pt = raw_pt x (placement_width_in / figure_width_in)
#
# and the second factor is invisible at the point where the font size is written.
# fig_opt2_timeline used to save 10.64 in wide and be placed at 7.9, turning a
# 6.4 pt provenance line into 4.8 pt on the screen -- two thirds of the size of
# the smallest text set directly in PowerPoint. Worse, bbox_inches="tight" grew
# the saved width to fit that very caption, so the longer the caption the smaller
# it rendered.
#
# So the placement width is now an argument, and every figure is measured against
# the deck's projection floor after it is drawn. The floor is set for a room
# where the back row is 6-7 m from the screen: on a 13.33 x 7.5 in slide, 9 pt is
# about 1/60 of the slide height, which is the conventional lower bound for
# supporting detail, and 10.5 pt (~1/50) is the bound for anything the audience is
# asked to read a number off.
# Raised from 9.0 when the body text went to 15 pt: 10 pt is two thirds of the
# body size, which is the usual floor for supporting type. Below that a label
# inside a plot reads as a footnote rather than part of the argument.
# The floor, the placement table and the report all live in deckgate.py, because
# there are two figure generators and for a long time only this one was checked.
MIN_EFF_PT = deckgate.MIN_EFF_PT
PLACE_W = deckgate.PLACE_W


def save(fig, name, place_w=None):
    """Write the PNG, then check every string in it against the projection floor.

    `place_w` overrides the width parsed out of the deck; it is only needed for a
    figure the deck places through something the parser cannot evaluate.
    """
    path = OUT / f"{name}.png"
    fig.savefig(path, dpi=DPI, transparent=False,
                bbox_inches="tight", pad_inches=0.08)
    deckgate.check(fig, path, name, DPI, place_w)
    plt.close(fig)


def legibility_report():
    deckgate.report()


def bare(ax, keep=("left", "bottom")):
    for s in ("top", "right", "left", "bottom"):
        ax.spines[s].set_visible(s in keep)


# ------------------------------------------------------ 0a. agreement study
def fig_spectra_valid():
    """Cluster-energy spectra of the three finders, and their ratio to the CPU.

    Data: validation_tiers.json, written by python/tests/validation_tiers.py —
    serial ClusterFinder, ClusterFinderFrozen and ClusterFinderCUDA over the
    same 10 000 frames from the same trained pedestal, 23.2 M clusters each.

    The overlay is deliberately unreadable as three curves: that is the result.
    The ratio panel is where the claim is testable, and it holds every populated
    bin inside +-0.1 %.
    """
    import json
    d = json.loads((Path(__file__).resolve().parent
                    / "validation_tiers.json").read_text())
    e = np.array(d["edges"])
    ctr = 0.5 * (e[1:] + e[:-1])
    h = {k: np.array(v) for k, v in d["hists"].items()}

    fig, (ax, axr) = plt.subplots(2, 1, figsize=(5.9, 3.9), sharex=True,
                                  gridspec_kw={"height_ratios": [2.6, 1]})
    for name, col, lw, ls in [("cpu", ACCENT, 2.0, "-"),
                              ("frozen", PALE, 1.3, "--"),
                              ("cuda", AMBER, 1.3, ":")]:
        ax.step(ctr, h[name], where="mid", color=col, lw=lw, ls=ls,
                label=f"{name}  ({d['totals'][name]:,})")
    ax.set_yscale("log")
    ax.set_ylim(3e2, 5e6)
    ax.legend(frameon=False, fontsize=10.6, labelcolor=TEXT2, loc="upper right")
    ax.set_ylabel("clusters / bin", fontsize=10.6)
    bare(ax, keep=("left", "bottom"))
    ax.set_title("cluster energy spectrum  ·  3×3, 10 000 frames, 23.2 M clusters",
                 color=MUTED, fontsize=10.6, loc="left", pad=6)

    m = h["cpu"] > 0
    axr.axhspan(0.999, 1.001, color=GREEN, alpha=0.18, zorder=1)
    axr.axhline(1.0, color=MUTED, lw=0.8, zorder=2)
    for name, col in [("frozen", PALE), ("cuda", AMBER)]:
        axr.plot(ctr[m], h[name][m] / h["cpu"][m], color=col, lw=1.1, zorder=3)
    dev = max(np.abs(h[n][m] / h["cpu"][m] - 1).max() for n in ("frozen", "cuda"))
    axr.set_ylim(0.9955, 1.0045)
    axr.set_yticks([0.996, 1.0, 1.004])
    axr.set_yticklabels(["−0.4 %", "0", "+0.4 %"], fontsize=10.6)
    axr.set_xlabel("cluster sum [ADU]", fontsize=10.6)
    axr.set_ylabel("vs CPU", fontsize=10.6)
    bare(axr, keep=("left", "bottom"))
    axr.text(0.985, 0.90, f"worst populated bin: {dev*100:.3f} %  ·  band = ±0.1 %",
             transform=axr.transAxes, ha="right", va="top", color=GREEN,
             fontsize=10.6)
    fig.subplots_adjust(hspace=0.10)
    save(fig, "fig_spectra_valid")


# ------------------------------------------------ 0. what a first run gives
def fig_first_run():
    """Achievable vs what a user gets on their first, naive run.

    The campaign's own 'cold' rep is NOT this number: the harness discards each
    chunk's clusters, so its result heap never grows and rep 0 shows only ~10^5
    faults. A user keeps their clusters. These are the retained, single-pass,
    one-process numbers recorded in python/tests/ClusterFinderCUDA_perf.ipynb
    (f32 build, 3x3, 100 000 frames), against the f32 campaign's warm ladder.

    The shape is the argument: the two ends of the ladder are untouched and
    everything between them loses a third. opt1 escapes only because it discards
    each frame as it goes and never grows the heap; opt6 escapes because it
    allocates nothing at all.
    """
    steps = ["opt1\n1 stream", "opt2\nstreams+batch", "opt3\nno barriers",
             "opt4\npinned", "opt5\nhost overlap", "opt6\nzero-copy"]
    warm = [15852, 25119, 29291, 40158, 50501, 61312]
    cold = [15773, 16651, 18961, 30642, 32342, 60587]
    faults = [0, 2626454, 2295417, 2294896, 2050300, 1]

    fig, ax = plt.subplots(figsize=(11.9, 3.35))
    x = np.arange(len(steps))
    w = 0.36
    TOP = 76000

    ax.bar(x - w / 2, warm, width=w, color=ACCENT, zorder=3, linewidth=0)
    ax.bar(x + w / 2, cold, width=w, color=AMBER, zorder=3, linewidth=0)

    # Floor = the best rate sustained unprofiled. nsys's own estimate of the H2D
    # engine occupancy is 60 140 FPS, 1.9 % lower, and corroborates it.
    ax.axhline(61312, color=GREEN, lw=1.2, ls="--", zorder=4)
    ax.text(2.6, 62100, "H2D floor · 61 312 FPS   (nsys estimates 60 140)",
            color=GREEN, fontsize=10.6, ha="center", va="bottom")

    for xi, (a, b, f) in enumerate(zip(warm, cold, faults)):
        ax.text(xi - w / 2, a + 900, f"{a:,}", ha="center", va="bottom",
                color=TEXT2, fontsize=10.6)
        ax.text(xi + w / 2, b + 900, f"{b:,}", ha="center", va="bottom",
                color=PALE, fontsize=11.2, fontweight="bold")
        drop = 100 * (1 - b / a)
        big = drop > 10
        ax.text(xi, -2600, f"{f:,} fault" + ("" if f == 1 else "s"),
                ha="center", va="top", color=AMBER if big else MUTED,
                fontsize=10.4, fontweight="bold" if big else "normal")
        ax.text(xi, -6300, ("−%.0f %%" % drop) if drop >= 1 else "—",
                ha="center", va="top", color=AMBER if big else MUTED,
                fontsize=11.8 if big else 10.6, fontweight="bold")

    # the two steps that pay nothing, annotated just above their own bars
    for xi, yi, why in [(0, 21500, "discards every frame\nas it goes"),
                        (5, 67500, "allocates nothing\nat all")]:
        ax.text(xi, yi, why, ha="center", va="bottom", color=GREEN,
                fontsize=10.4, linespacing=1.35)

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=10.6, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    ax.tick_params(axis="x", pad=34)
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)

    handles = [Rectangle((0, 0), 1, 1, color=ACCENT),
               Rectangle((0, 0), 1, 1, color=AMBER)]
    ax.legend(handles, ["achievable · warm, 5-rep campaign",
                        "first run · results retained, one process"],
              frameon=False, fontsize=10.6, labelcolor=TEXT2, loc="upper left",
              bbox_to_anchor=(0.0, 1.055), ncol=2, handlelength=1.1)
    ax.set_title("frames / second   ·   3×3, 100 000 frames, f32   ·   "
                 "minor faults and the throughput they cost, per step",
                 color=MUTED, fontsize=10.6, loc="left", pad=8)
    save(fig, "fig_first_run")


# ------------------------------------------------------- 1. the arc, 3x3
def fig_arc():
    """The whole ladder at 3x3. Acts I-II on f64, opt7 is the f32 flip."""
    steps = ["CPU MT\n24 threads", "opt1\n1 stream", "opt2\nstreams+batch",
             "opt3\nno barriers", "opt4\npinned", "opt5\nhost overlap",
             "opt6\nzero-copy", "opt7\nf32 kernel"]
    # CPU bar = the BEST thread count, not the campaign's original 48. This is a
    # 16-core / 32-thread 7950X, so 48 oversubscribed it by 1.5x and understated
    # the CPU by 29 % (cpu_threads.csv). One baseline for every bar: the CPU
    # finder does not depend on DEVICE_PED_TYPE, so per-arm baselines only ever
    # encoded run-to-run noise.
    fps = [6762, 15807, 24726, 29188, 38486, 50410, 58495, 61312]
    spd = [1.0, 2.34, 3.66, 4.32, 5.69, 7.45, 8.65, 9.07]
    colors = [MUTED] + [ACCENT] * 4 + [PALE] * 2 + [AMBER]

    fig, ax = plt.subplots(figsize=(11.4, 3.8))
    x = np.arange(len(steps))
    TOP = 78000

    # act bands, behind the bars, labelled along the top
    # Short labels only. The gloss ("feed the GPU") is wider than the one-bar
    # ACT III band, so spelling it out here puts ACT III's label on ACT II's.
    for x0, x1, label, col in [(-0.5, 4.5, "ACT I", ACCENT),
                               (4.5, 6.5, "ACT II", PALE),
                               (6.5, 7.5, "ACT III", AMBER)]:
        ax.axvspan(x0, x1, color=col, alpha=0.05, zorder=0)
        ax.plot([x0 + 0.08, x1 - 0.08], [TOP * 0.955] * 2, color=col, lw=2.2,
                zorder=2)
        ax.text((x0 + x1) / 2, TOP * 0.965, label, ha="center", va="bottom",
                color=col, fontsize=10.6, fontweight="bold")

    ax.bar(x, fps, width=0.62, color=colors, zorder=3, linewidth=0)

    # The H2D floor: 61 859 FPS on f64 (the nsys estimate, which this arm never
    # reached -- opt6 stops 5.4 % short) and 61 312 on f32 (the best rate actually
    # sustained). One band at this scale.
    ax.axhspan(61312, 61859, color=GREEN, alpha=0.20, zorder=1)
    ax.axhline(61859, color=GREEN, lw=1.2, ls="--", zorder=4)
    ax.text(-0.42, 63200, "H2D floor · 61–62 k FPS · the GPU cannot be fed faster",
            color=GREEN, fontsize=10.6, ha="left", va="bottom")

    for xi, (f, s) in enumerate(zip(fps, spd)):
        ax.text(xi, f + 1100, f"{f:,}", ha="center", va="bottom",
                color=PALE, fontsize=12.4, fontweight="bold")
        ax.text(xi, f - 1600, ("base" if s == 1.0 else f"×{s:.2f}"),
                ha="center", va="top", color=BG, fontsize=10.6, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=10.6, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("frames / second   ·   3×3 clusters, 100 000 frames, warm run   ·   "
                 "every bar against the best CPU configuration (24 threads)",
                 color=MUTED, fontsize=10.6, loc="left", pad=8)
    save(fig, "fig_arc")


# --------------------------------------------------- 2. the arc, 9x9
def fig_arc_9x9():
    """9x9 is where Acts II and III actually pay. Same axes, different regime."""
    steps = ["CPU MT\n32 threads", "opt3\nno barriers", "opt4\npinned",
             "opt5\nhost overlap", "opt6\nzero-copy", "opt7\nf32 kernel"]
    # Best thread count at 9x9 is 32, not the 24 that wins at 3x3: the drain of
    # ClusterCollector scales with thread count and 9x9 clusters are 9x larger,
    # so the two sizes optimize differently (cpu_threads.csv).
    # cap 1700, not 1500: the campaign's 9x9 cap was BELOW the per-frame maximum
    # (1633) and silently truncated 0.0095 % of clusters. results/2026-08-20_*.
    fps = [1503, 12129, 12527, 15063, 33323, 39775]
    spd = [1.0, 8.07, 8.33, 10.02, 22.17, 26.46]
    colors = [MUTED] + [ACCENT] * 2 + [PALE] * 2 + [AMBER]

    fig, ax = plt.subplots(figsize=(11.4, 3.8))
    x = np.arange(len(steps))
    TOP = 62000

    for x0, x1, label, col in [(-0.5, 2.5, "ACT I", ACCENT),
                               (2.5, 4.5, "ACT II", PALE),
                               (4.5, 5.5, "ACT III", AMBER)]:
        ax.axvspan(x0, x1, color=col, alpha=0.05, zorder=0)
        ax.plot([x0 + 0.08, x1 - 0.08], [TOP * 0.955] * 2, color=col, lw=2.2,
                zorder=2)
        ax.text((x0 + x1) / 2, TOP * 0.965, label, ha="center", va="bottom",
                color=col, fontsize=10.6, fontweight="bold")

    ax.bar(x, fps, width=0.58, color=colors, zorder=3, linewidth=0)

    # Here the floor MOVES -- and CHANGES ENGINE, which is the point of Act III.
    #
    # On both arms the sustained rate beats the profiled estimate, so the sustained
    # rate IS the floor (see module docstring); the probe's independent estimates
    # are printed alongside so the floor is not merely the best bar restated.
    #
    # At cap 1700 the f64 kernel (32.66 us) is taller than BOTH transfers, so it
    # still binds. opt7 cuts it 40 % to 23.94 -- below the 25.24 us D2H bar -- so
    # the f32 floor is D2H, not the kernel. The kernel optimization succeeded so
    # completely that it handed the constraint to the result path.
    ax.hlines(33323, -0.5, 4.5, color=GREEN, lw=1.3, ls="--", zorder=4)
    ax.text(-0.42, 33900, "f64 KERNEL floor · 33 323 FPS   (nsys estimates 30 621)",
            color=GREEN, fontsize=10.6, va="bottom")
    ax.hlines(39775, 4.5, 5.5, color=AMBER, lw=1.3, ls="--", zorder=4)
    ax.text(5.46, 45000, "f32 D2H floor · 39 775 FPS\n(nsys estimates 39 614)",
            color=AMBER, fontsize=10.6, ha="right", va="bottom", linespacing=1.35)
    ax.add_patch(FancyArrowPatch((4.62, 34100), (4.62, 39100), arrowstyle="-|>",
                                 mutation_scale=11, color=AMBER, lw=1.5, zorder=5))
    ax.text(2.55, 51000, "−40 % kernel → D2H binds instead", color=AMBER,
            fontsize=10.6, ha="center", va="center", fontweight="bold")

    for xi, (f, s) in enumerate(zip(fps, spd)):
        ax.text(xi, f + 800, f"{f:,}", ha="center", va="bottom",
                color=PALE, fontsize=12.4, fontweight="bold")
        ax.text(xi, f - 1100, ("base" if s == 1.0 else f"×{s:.2f}"),
                ha="center", va="top", color=BG, fontsize=10.6, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=10.6, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("frames / second   ·   9×9, 20 000 frames, cap 1700 (lossless)   ·   "
                 "best CPU configuration (32 threads)   ·   opt1/opt2 are 3×3 only",
                 color=MUTED, fontsize=10.6, loc="left", pad=8)
    save(fig, "fig_arc_9x9")


# ------------------------------------------- 3. where the time actually went
def fig_overhead():
    """GPU floor vs host excess. Act II collapses the host bar; Act III lowers
    the floor underneath it."""
    fig, axes = plt.subplots(1, 2, figsize=(11.4, 3.2))

    panels = [
        # Floor = PEAK as the deck defines it (lower of probe and best sustained),
        # so opt6/opt7 sit exactly ON their floor rather than under it. 9x9 is the
        # cap-1700 re-take, where the f32 floor is D2H and not the kernel.
        (axes[0], "3×3  ·  H2D-bound",
         ["opt3", "opt4", "opt5", "opt6", "opt7"],
         [16.17, 16.17, 16.17, 16.17, 16.31],           # floor
         [34.26, 25.98, 19.84, 17.10, 16.31], 48),      # measured
        (axes[1], "9×9  ·  kernel-bound, then D2H",
         ["opt3", "opt4", "opt5", "opt6", "opt7"],
         [30.01, 30.01, 30.01, 30.01, 25.14],
         [82.44, 79.83, 66.39, 30.01, 25.14], 116),
    ]

    for ax, title, steps, floor, meas, ymax in panels:
        x = np.arange(len(steps))
        excess = [max(0.0, m - f) for m, f in zip(meas, floor)]
        cols = [ACCENT, ACCENT, PALE, PALE, AMBER]
        ax.bar(x, floor, width=0.56, color=cols, zorder=3, linewidth=0)
        ax.bar(x, excess, width=0.56, bottom=floor, color=MUTED, zorder=3,
               linewidth=0, alpha=0.55)
        for xi, (f, e, m) in enumerate(zip(floor, excess, meas)):
            ax.text(xi, m + ymax * 0.022, f"{m:.1f}", ha="center", color=PALE,
                    fontsize=11.2, fontweight="bold")
            if e > ymax * 0.05:
                ax.text(xi, f + e / 2, f"+{e:.0f}", ha="center", va="center",
                        color=BG, fontsize=10.6, fontweight="bold")
        ax.set_xticks(x)
        ax.set_xticklabels(steps, color=TEXT2, fontsize=10.6)
        ax.set_ylim(0, ymax)
        ax.set_yticks([])
        bare(ax, keep=("bottom",))
        ax.set_title(title, color=PALE, fontsize=11.8, pad=8, loc="left")

    axes[0].set_ylabel("µs / frame", color=TEXT2, fontsize=10.6)
    # label the two segments in place — the floor takes the act colour, so a
    # colour-keyed legend would be wrong
    axes[0].text(0, 16.17 / 2, "GPU\nfloor", ha="center", va="center", color=BG,
                 fontsize=10.6, fontweight="bold")
    axes[0].annotate("host excess", xy=(0.30, 25), xytext=(1.15, 41),
                     color=TEXT2, fontsize=10.6,
                     arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.9))

    axes[1].text(1.5, 108, "Act II removes the host bar", color=PALE, fontsize=10.6,
                 ha="center", fontweight="bold")
    axes[1].add_patch(FancyArrowPatch((1.5, 104), (3.25, 34), arrowstyle="-|>",
                                      mutation_scale=10, color=PALE, lw=1.3,
                                      connectionstyle="arc3,rad=-0.22"))
    axes[1].text(4.0, 52, "Act III lowers\nthe floor", color=AMBER, fontsize=10.6,
                 ha="center", fontweight="bold")
    axes[1].add_patch(FancyArrowPatch((4.0, 46), (4.0, 27), arrowstyle="-|>",
                                      mutation_scale=10, color=AMBER, lw=1.3))
    save(fig, "fig_overhead")


# ------------------------------------------------------ 4. streams timeline
# 3x3 [f64 · s1] proportions, 1 unit = 1 us: H2D 13.14, kernel 14.72, D2H 5.31.
# These slides are 3x3-only, and at 3x3 H2D is the tallest bar -- which the
# schedule below then reproduces on its own rather than being asserted.
H_, K_, D_ = 13, 15, 5
LANE_ = 0.68


def _frame_bars(ax, y, t0):
    ax.broken_barh([(t0, H_)], (y, LANE_), facecolors=AMBER, zorder=3)
    ax.broken_barh([(t0 + H_, K_)], (y, LANE_), facecolors=ACCENT, zorder=3)
    ax.broken_barh([(t0 + H_ + K_, D_)], (y, LANE_), facecolors=PALE, zorder=3)


def _schedule(n_frames, n_streams, H=None, K=None, D=None, round_size=None):
    """Greedy list schedule that honours the real engine constraints.

    H2D and D2H are ONE FIFO resource each -- the GPU has a single copy engine
    per direction, which is why H2D_overlap and D2H_overlap are 1.00 in every
    row of probes.csv. Kernels may overlap (measured 1.02-1.32x). A frame's
    three stages stay ordered, and a stream cannot start its next frame until
    its previous D2H has freed the per-stream buffers.

    round_size=n reproduces opt2's cudaDeviceSynchronize() after every round of
    n frames: every resource resets to the round's end, so the drain is a
    consequence of the barrier rather than a drawn-in gap.

    Returns (frames, drains) with frames = [(stream, h0, k0, d0)] and drains =
    [(t_h2d_goes_idle, t_round_end)] -- exactly the time the barrier costs.
    """
    H, K, D = H or H_, K or K_, D or D_
    h_free = d_free = 0.0
    ready = [0.0] * n_streams
    frames, drains = [], []
    last_h_end = 0.0
    for i in range(n_frames):
        if round_size and i and i % round_size == 0:
            t = max(max(ready), h_free, d_free)
            drains.append((last_h_end, t))
            h_free = d_free = t
            ready = [t] * n_streams
        s = i % n_streams
        h0 = max(ready[s], h_free); h_free = last_h_end = h0 + H
        k0 = h_free; k1 = k0 + K
        d0 = max(k1, d_free); d_free = ready[s] = d0 + D
        frames.append((s, h0, k0, d0))
    if round_size:
        drains.append((last_h_end, max(max(ready), h_free, d_free)))
    return frames, drains


def _draw_schedule(ax, frames, top_lane=3.0, H=None, K=None, D=None):
    H, K, D = H or H_, K or K_, D or D_
    for s, h0, k0, d0 in frames:
        y = top_lane - s * 1.0
        ax.broken_barh([(h0, H)], (y, LANE_), facecolors=AMBER, zorder=3)
        ax.broken_barh([(k0, K)], (y, LANE_), facecolors=ACCENT, zorder=3)
        ax.broken_barh([(d0, D)], (y, LANE_), facecolors=PALE, zorder=3)


def fig_streams():
    fig, axes = plt.subplots(3, 1, figsize=(7.7, 2.85))
    FR = H_ + K_ + D_

    # --- opt1: one stream, strictly serial
    ax = axes[0]
    for i in range(3):
        _frame_bars(ax, 1.0, i * FR)
    ax.set_ylim(0.4, 2.3)
    ax.text(3 * FR + 6, 1.34, "one engine at a time", color=MUTED, fontsize=10.6,
            va="center")

    # --- opt2: 4 streams, barrier after each round. The drain is not drawn in;
    # it falls out of the schedule, because after the round's last H2D the copy
    # engine has nothing left to feed until the barrier releases.
    ax = axes[1]
    frames, drains = _schedule(8, 4, round_size=4)
    _draw_schedule(ax, frames)
    for h_idle, t_end in drains:
        ax.axvspan(h_idle, t_end, color=AMBER, alpha=0.13, zorder=1)
    # The label has to clear the top lane AND stay out of the axes title, which
    # is anchored to the axes box and so moves with the LIMITS, not with the
    # data. Extra headroom in y is what separates the two.
    ax.text((drains[0][0] + drains[0][1]) / 2, 4.05, "barrier — H2D starves",
            color=AMBER, fontsize=10.6, ha="center", va="bottom")
    ax.set_ylim(-0.4, 5.6)

    # --- opt3: no barriers, continuous
    ax = axes[2]
    frames, _ = _schedule(11, 4)
    _draw_schedule(ax, frames)
    ax.set_ylim(-1.5, 4.5)
    ax.text(0, -0.25, "streams never wait on each other — the H2D engine never goes idle",
            color=ACCENT, fontsize=10.6, va="top")
    titles = ["opt1  ·  1 stream, synchronous",
              "opt2  ·  4 streams, sync barrier per round",
              "opt3  ·  4 streams, barriers removed"]
    for ax, t in zip(axes, titles):
        ax.set_xlim(-2, 178)
        ax.set_yticks([]); ax.set_xticks([])
        bare(ax, keep=())
        ax.set_title(t, color=TEXT2, fontsize=10.6, loc="left", pad=4)

    handles = [Rectangle((0, 0), 1, 1, color=c) for c in (AMBER, ACCENT, PALE)]
    axes[0].legend(handles, ["H2D copy", "kernel", "D2H copy"], frameon=False,
                   fontsize=10.6, labelcolor=TEXT2, ncol=3, loc="lower right",
                   bbox_to_anchor=(1.02, 0.98), handlelength=1.1)
    # "time →" inside the axes, at the far end: as an xlabel it sat under the
    # left edge, on top of the sentence already written there.
    axes[2].text(176, -0.25, "time  →", color=MUTED, fontsize=10.6,
                 ha="right", va="top")
    fig.subplots_adjust(hspace=0.75, bottom=0.10)
    save(fig, "fig_streams")


# ------------------------- 6b. the big picture: two memories and a slow wire
def fig_gpu_model():
    """What "H2D" and "D2H" actually name, for a room that has never used a GPU.

    Deliberately the least quantitative figure in the deck. Its only job is that
    a kernel addresses VRAM and nothing else, so every frame is copied in and
    every result copied out -- and that the wire carrying those copies is ~32x
    slower than the memory at either end. Every step in Act I and Act II is an
    attack on one of the two arrows, so the audience needs the picture before
    the ladder starts, not after.
    """
    fig, ax = plt.subplots(figsize=(11.6, 3.4))

    def panel(x0, x1, y0, y1, title, col):
        ax.add_patch(FancyBboxPatch((x0, y0), x1 - x0, y1 - y0,
                                    boxstyle="round,pad=0,rounding_size=1.2",
                                    facecolor=PANEL, edgecolor=col,
                                    linewidth=1.6, zorder=2))
        ax.text((x0 + x1) / 2, y1 - 2.2, title, ha="center", va="center",
                color=col, fontsize=13.0, fontweight="bold")

    def chip(x0, x1, y0, y1, top, bottom, col):
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor=BG,
                               edgecolor=col, linewidth=1.3, zorder=3))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2 + 1.6, top, ha="center",
                va="center", color=PALE, fontsize=13.0, fontweight="bold",
                zorder=4)
        ax.text((x0 + x1) / 2, (y0 + y1) / 2 - 1.9, bottom, ha="center",
                va="center", color=TEXT2, fontsize=11.8, zorder=4)

    panel(1, 33, 3, 44, "HOST", MUTED)
    chip(4, 30, 31, 40, "CPU", "24 threads", ACCENT)
    chip(4, 30, 9, 18, "DRAM", "system memory", ACCENT)

    panel(67, 99, 3, 44, "DEVICE  ·  RTX 4090", MUTED)
    chip(70, 96, 31, 40, "128 SMs", "16 384 CUDA cores", AMBER)
    chip(70, 96, 9, 18, "VRAM", "24 GB GDDR6X", AMBER)

    # the fast links, inside each box
    for x, lab, col in ((17, "~71 GB/s", ACCENT), (83, "1 008 GB/s", AMBER)):
        ax.annotate("", xy=(x, 30.4), xytext=(x, 18.6),
                    arrowprops=dict(arrowstyle="<|-|>", color=col, lw=1.4))
        if lab:
            ax.text(x + 1.4, 24.5, lab, ha="left", va="center", color=col,
                    fontsize=12.4, fontweight="bold")
    ax.text(17, 5.2, "off-limits to the kernel",
            ha="center", va="center", color=MUTED, fontsize=12.4)

    # the slow wire, between them
    ax.annotate("", xy=(69, 16.5), xytext=(31, 16.5),
                arrowprops=dict(arrowstyle="-|>", color=GREEN, lw=2.4))
    ax.text(50, 18.4, "H2D   the frame, 312.5 kB", ha="center", va="bottom",
            color=GREEN, fontsize=13.0, fontweight="bold")
    ax.annotate("", xy=(31, 10.5), xytext=(69, 10.5),
                arrowprops=dict(arrowstyle="-|>", color=PALE, lw=2.4))
    ax.text(50, 8.6, "D2H   the clusters, 93 kB at 3×3", ha="center", va="top",
            color=PALE, fontsize=13.0, fontweight="bold")
    # The middle column carries four separate strings between the two panels.
    # They are spaced off the arrows below them, not off each other: at 13 pt in
    # a 34-unit gap, two of them a row apart is already a collision.
    ax.text(50, 37.5, "PCIe 4.0 ×16", ha="center", va="center", color=TEXT2,
            fontsize=13.0, fontweight="bold")
    ax.text(50, 33.0, "31.5 GB/s", ha="center", va="center", color=TEXT2,
            fontsize=13.0)
    ax.text(50, 28.0, "the narrowest link in the chain", ha="center",
            va="center", color=MUTED, fontsize=12.4)

    ax.set_xlim(-1, 101); ax.set_ylim(1, 46)
    ax.axis("off")
    save(fig, "fig_gpu_model")


# ------------------------------------------------------------- 5. pinning
def fig_pinning():
    fig = plt.figure(figsize=(7.7, 2.6))
    ax = fig.add_axes([0, 0.04, 0.655, 0.96]); ax.axis("off")
    # 11.6 units across 5.04 in is 0.435 in per unit; every box below is sized
    # so its longest string fits INSIDE it at 11.2 pt, and every arrow label
    # sits above the boxes rather than in the gap between them, because the gap
    # is narrower than the words that were being put in it.
    ax.set_xlim(0, 11.6); ax.set_ylim(0, 7.0)

    def box(x, y, w, h, label, sub=""):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=PANEL, edgecolor=RULE, lw=1))
        ax.text(x + w / 2, y + h / 2 + 0.24, label, ha="center", va="center",
                color=PALE, fontsize=11.2, fontweight="bold")
        ax.text(x + w / 2, y + h / 2 - 0.32, sub, ha="center", va="center",
                color=MUTED, fontsize=11.2)

    def arrow(x0, x1, y, color, label, ytext):
        ax.add_patch(FancyArrowPatch((x0, y), (x1, y), arrowstyle="-|>",
                                     mutation_scale=10, color=color, lw=1.6))
        ax.text((x0 + x1) / 2, ytext, label, ha="center", va="bottom",
                color=color, fontsize=11.2)

    ax.text(0, 6.52, "PAGEABLE  ·  before opt4", color=AMBER, fontsize=11.2,
            fontweight="bold")
    box(0, 4.10, 3.0, 1.05, "numpy array", "pageable")
    box(4.3, 4.10, 3.0, 1.05, "driver staging", "hidden buffer")
    box(8.6, 4.10, 3.0, 1.05, "GPU", "device memory")
    arrow(3.0, 4.3, 4.62, AMBER, "memcpy", 5.35)
    arrow(7.3, 8.6, 4.62, AMBER, "DMA", 5.35)
    ax.text(0, 3.64, "every transfer is copied twice", color=MUTED, fontsize=11.2)

    ax.text(0, 2.86, "PINNED  ·  opt4", color=ACCENT, fontsize=11.2, fontweight="bold")
    box(0, 0.90, 3.0, 1.05, "numpy array", "page-locked")
    box(8.6, 0.90, 3.0, 1.05, "GPU", "device memory")
    arrow(3.0, 8.6, 1.42, ACCENT, "DMA · reads host RAM directly", 2.15)
    ax.text(0, 0.42, "no staging copy, no page faults, fully async",
            color=MUTED, fontsize=11.2)

    # the rule, in one inset: pinning pays only where H2D is the tallest bar
    ax2 = fig.add_axes([0.745, 0.16, 0.255, 0.62])
    x = np.arange(2)
    # 3x3 from 2026-08-18_f64, 9x9 from 2026-08-20_f64_cap1700 -- NOT the
    # cap-1500 ladder, which read 82.17 -> 80.44 and put a stale x1.02 on the
    # slide for a step that actually buys x1.03.
    before = [34.26, 82.44]
    after = [25.98, 79.83]
    ax2.bar(x - 0.19, before, width=0.36, color=AMBER, zorder=3, label="opt3")
    ax2.bar(x + 0.19, after, width=0.36, color=ACCENT, zorder=3, label="opt4")
    for xi, (b, a) in enumerate(zip(before, after)):
        ax2.text(xi - 0.19, b + 2, f"{b:.0f}", ha="center", color=TEXT2, fontsize=11.2)
        ax2.text(xi + 0.19, a + 2, f"{a:.0f}", ha="center", color=TEXT2, fontsize=11.2)
        ax2.text(xi, 92, f"×{b / a:.2f}", ha="center", color=PALE, fontsize=11.2,
                 fontweight="bold")
    ax2.set_xticks(x)
    ax2.set_xticklabels(["3×3\nH2D-bound", "9×9\nkernel-bound"], color=TEXT2,
                        fontsize=11.2)
    ax2.set_ylim(0, 104); ax2.set_yticks([]); bare(ax2, keep=("bottom",))
    ax2.set_title("µs / frame", color=MUTED, fontsize=11.2, pad=6)
    ax2.legend(frameon=False, fontsize=11.2, labelcolor=TEXT2, loc="center left")
    save(fig, "fig_pinning")


# ----------------------------------------------- 6. graphs (rejected route)
def fig_graphs():
    fig = plt.figure(figsize=(7.6, 2.55))
    ax = fig.add_axes([0, 0.07, 0.66, 0.90])
    ax.axis("off"); ax.set_xlim(0, 11.2); ax.set_ylim(0, 4.6)

    def node(x, y, w, h, t, fc):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor="none"))
        ax.text(x + w / 2, y + h / 2, t, ha="center", va="center",
                color=BG, fontsize=11.2, fontweight="bold")

    ops = [("H2D", AMBER), ("kernel", ACCENT), ("D2H", PALE)] * 2

    ax.text(0, 4.15, "WITHOUT GRAPHS  ·  one driver call per operation, every frame",
            color=AMBER, fontsize=11.2, fontweight="bold")
    for i, (t, c) in enumerate(ops):
        x = 0.1 + i * 1.62
        node(x, 2.85, 1.4, 0.6, t, c)
        ax.add_patch(FancyArrowPatch((x + 0.7, 3.72), (x + 0.7, 3.52),
                                     arrowstyle="-|>", mutation_scale=7,
                                     color=MUTED, lw=0.9))
    ax.text(11.1, 3.15, "CPU cost\n≈ 6 launches", ha="right", va="center",
            color=MUTED, fontsize=11.2)

    ax.text(0, 2.18, "WITH GRAPHS  ·  record once, replay with one launch",
            color=ACCENT, fontsize=11.2, fontweight="bold")
    ax.add_patch(Rectangle((0.1, 0.72), 9.20, 1.15, facecolor=PANEL,
                           edgecolor=ACCENT, lw=1.2))
    for i, (t, c) in enumerate(ops):
        node(0.32 + i * 1.48, 0.98, 1.26, 0.6, t, c)
    ax.add_patch(FancyArrowPatch((0.8, 2.02), (0.8, 1.90), arrowstyle="-|>",
                                 mutation_scale=8, color=ACCENT, lw=1.3))
    ax.text(11.1, 1.30, "CPU cost\n≈ 1 launch", ha="right", va="center",
            color=ACCENT, fontsize=11.2, fontweight="bold")

    # the verdict
    ax2 = fig.add_axes([0.73, 0.13, 0.27, 0.78])
    x = np.arange(2)
    opt4 = [25.98, 79.83]          # 3x3 2026-08-18_f64 · 9x9 2026-08-20_f64_cap1700
    graph = [25.16, 90.32]         # cap 1500 read 80.44 / 94.78 and said "18 % slower"
    ax2.bar(x - 0.19, opt4, width=0.36, color=ACCENT, zorder=3, label="opt4")
    ax2.bar(x + 0.19, graph, width=0.36, color=AMBER, zorder=3, label="graphs")
    for xi, (a, g) in enumerate(zip(opt4, graph)):
        ax2.text(xi - 0.19, a + 2.5, f"{a:.0f}", ha="center", color=TEXT2, fontsize=11.2)
        ax2.text(xi + 0.19, g + 2.5, f"{g:.0f}", ha="center", color=TEXT2, fontsize=11.2)
    ax2.text(1, 100, "−12 % THROUGHPUT", ha="center", color=AMBER, fontsize=11.2,
             fontweight="bold")
    ax2.set_xticks(x)
    ax2.set_xticklabels(["3×3", "9×9"], color=TEXT2, fontsize=11.2)
    ax2.set_ylim(0, 120); ax2.set_yticks([]); bare(ax2, keep=("bottom",))
    ax2.set_title("µs / frame", color=MUTED, fontsize=11.2, pad=6)
    ax2.legend(frameon=False, fontsize=11.2, labelcolor=TEXT2, loc="upper left")

    save(fig, "fig_graphs")


# -------------------------------------- 7. the result path (Act II, opt5/opt6)
def fig_resultpath():
    """Why zero-copy is worth x1.16 at 3x3 and x2.21 at 9x9: whether the host
    copy fits underneath the GPU floor."""
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11.4, 3.2))

    for ax, title, floor, copy_us, gain, verdict, col in [
        (a1, "3×3  ·  host copy 93 kB / frame", 16.17, 8.0, "×1.16",
         "the copy hides under the GPU floor  →  small win", ACCENT),
        (a2, "9×9  ·  host copy 467 kB / frame", 30.01, 62.0, "×2.21",
         "the copy is larger than the floor  →  cannot hide", AMBER),
    ]:
        # 1.35 apart, not 1.0: at 11.8 pt each two-line tick label is about
        # 0.95 x-units wide, so adjacent bars put them into each other.
        ax.bar([0], [floor], width=0.5, color=col, zorder=3, linewidth=0)
        ax.bar([1.35], [copy_us], width=0.5, color=col, zorder=3, linewidth=0)
        ax.axhline(floor, color=GREEN, lw=1.3, ls="--", zorder=4)
        # ABOVE the line and to the right of both bars. On the line it read as
        # struck through; at the left edge it met a bar's own value label.
        ax.text(2.55, floor + 1.2, "GPU floor", color=GREEN, fontsize=11.8,
                ha="right", va="bottom")
        ax.text(0, floor + 2.1, f"{floor:.1f} µs", ha="center", color=PALE,
                fontsize=11.8, fontweight="bold")
        ax.text(1.35, copy_us + 2.1, f"≈{copy_us:.0f} µs", ha="center", color=PALE,
                fontsize=11.8, fontweight="bold")
        ax.set_xticks([0, 1.35])
        ax.set_xticklabels(["GPU per frame\nH2D ∥ kernel ∥ D2H",
                            "host copy per frame\ncollect() memcpy"],
                           color=TEXT2, fontsize=11.8)
        ax.set_xlim(-0.7, 2.6); ax.set_ylim(0, 82); ax.set_yticks([])
        bare(ax, keep=("bottom",))
        # The verdict is the second line of the TITLE. As free-floating text it
        # had nowhere to go: above the bars it met the value labels, below the
        # axis it met two-line tick labels, and beside them it met the gain.
        ax.set_title(f"{title}\n{verdict}", color=PALE, fontsize=11.8, pad=10,
                     loc="left", linespacing=1.5)
        ax.text(2.55, 76, gain, color=col, fontsize=18.9, fontweight="bold", ha="right")
        ax.text(2.55, 69, "opt5 → opt6", color=MUTED, fontsize=11.8, ha="right")
    fig.subplots_adjust(bottom=0.24, top=0.78)
    save(fig, "fig_resultpath")


# ------------------------------------------------ 8. f32 kernel (nsys truth)
def fig_f32_kernel():
    """The same three engines, read at s1 and at s4 — cap 1700, both arms.

    s1 is the DURATION claim: overlap is exactly 1.00 on every engine, so the
    union per frame IS the mean duration, and the kernel is tallest in BOTH
    arms. s4 is the BINDING claim: the copy engines still serialize (overlap
    1.00) but the kernel overlaps itself across streams, so its per-frame
    number is occupancy. Only at s4 does the f32 kernel fall below D2H, which
    is why the left panel cannot be used to argue the handover.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.4, 2.8))
    labels = ["kernel", "D2H", "H2D"]
    y = np.arange(3); h = 0.34
    panels = [
        (ax1, "s1 · UNCONTENDED  —  duration",
         [39.86, 21.97, 13.20], [23.70, 21.95, 13.22], "−40.5 %",
         "kernel binds in BOTH arms"),
        (ax2, "s4 · SHIPPED  —  what binds",
         [32.66, 25.25, 20.77], [23.94, 25.24, 20.54], "−26.7 %",
         "f32 kernel drops BELOW D2H"),
    ]
    for ax, title, f64, f32, delta, verdict in panels:
        ax.barh(y + h / 2, f64, height=h, color=ACCENT, zorder=3, label="f64 ped")
        ax.barh(y - h / 2, f32, height=h, color=AMBER, zorder=3, label="f32 ped")
        # the engine that sets the floor in each arm is named, not ringed
        i64, i32 = int(np.argmax(f64)), int(np.argmax(f32))
        for yi, (a, b) in enumerate(zip(f64, f32)):
            for val, off, top in ((a, +h / 2, yi == i64), (b, -h / 2, yi == i32)):
                # the kernel row carries the argument, so it stays legible even
                # in the arm where it no longer binds
                ax.text(val + 0.7, yi + off, f"{val:.2f}" + ("  ◀ binds" if top else ""),
                        va="center", color=PALE if (top or yi == 0) else TEXT2,
                        fontsize=11.8, fontweight="bold" if top else "normal")
        # A bar pair spans y +- h, so the delta and the verdict need a full h of
        # clearance beyond that or they sit on the bar they are labelling. The
        # y limits carry that margin explicitly rather than relying on the
        # default padding, which the larger type had eaten.
        ax.text(f64[0] * 0.42, -h - 0.30, delta, ha="center", va="bottom",
                color=PALE, fontsize=12.4, fontweight="bold")
        ax.text(0.985, 2 + h + 0.34, verdict, transform=ax.get_yaxis_transform(),
                ha="right", va="top", color=PALE, fontsize=11.8, fontweight="bold")
        ax.set_yticks(y); ax.set_yticklabels(labels, color=TEXT2, fontsize=11.8)
        # "39.86  ◀ binds" is ~22 x-units of text starting at the bar's end, so
        # the axis has to be long enough to hold the label as well as the bar.
        ax.invert_yaxis(); ax.set_xlim(0, 80); ax.set_xticks([])
        ax.set_ylim(3.05, -0.98)
        bare(ax, keep=("left",))
        ax.set_title(title, color=PALE, fontsize=11.8, pad=10, loc="left")

    # the wall the f32 kernel has to clear, drawn only where it matters
    ax2.axvline(25.24, color=MUTED, lw=1.0, ls="--", zorder=1)   # under the labels
    # parked between the D2H and H2D rows, the one region no label reaches
    ax1.legend(frameon=False, fontsize=11.8, labelcolor=TEXT2, loc="center right",
               bbox_to_anchor=(1.0, 0.40))
    fig.subplots_adjust(bottom=0.20, top=0.86, left=0.10, right=0.99, wspace=0.34)
    save(fig, "fig_f32_kernel")


# --------------------------------------------------------- 9. cancellation
def fig_cancellation():
    """The trap and what it did to the physics, on one pair of axes.

    LEFT is the cancellation itself: two ~2.17e7 operands, an answer of ~2025 for
    a normal pixel and ~9 for a quiet one, against a fixed +-3 ADU^2 f32 error
    that does not shrink with the answer.

    RIGHT is the consequence. The f64/CPU curve is MEASURED -- validation_tiers.json,
    23.2 M clusters over 10 000 frames. The f32 curve is RECONSTRUCTED, not
    measured: the excess is placed where SS9 of
    docs/pedestal_precision_f32_cancellation.md derives it (a threshold that has
    collapsed from ~225 ADU to ~0 admits the whole positive side of the pixel's
    distribution, so the population is smeared upward from zero) and its AREA is
    set to the measured +28.06 % from SS1. Shape schematic, area measured, and the
    caption on the slide says so. Drawing a measured f32 spectrum would be better;
    that build is not one anyone should keep around to re-run.

    The variance-vs-rms error floor that used to sit in this panel is now
    fig_varfloor, in the annex: it is the quantitative version of the same claim
    and it was competing with the physics for the audience's attention.
    """
    import json
    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(7.9, 2.34),
                                  gridspec_kw={"width_ratios": [1.0, 1.28]})

    names = ["E[X²]\n2.17e7", "mean²\n2.17e7", "var\nbulk 2025", "var\nquiet 9"]
    ax.bar([0, 1], [2.17e7, 2.17e7], width=0.52, color=[PALE, PALE], zorder=3)
    ax.bar([2], [2025], width=0.52, color=AMBER, zorder=3)
    ax.bar([3], [9], width=0.52, color=AMBER, zorder=3)
    # Headroom above the 2.17e7 bars, so the note can sit over empty axes
    # instead of over the two operands it is talking about.
    ax.set_yscale("log"); ax.set_ylim(1, 3e11)
    ax.set_xticks([0, 1, 2, 3])
    ax.set_xticklabels(names, color=TEXT2, fontsize=10.6, linespacing=1.35)
    ax.set_yticks([1e0, 1e2, 1e4, 1e6, 1e8])
    ax.set_xlim(-0.6, 3.7)
    ax.tick_params(labelsize=10.6)
    ax.axhline(3, color=ACCENT, lw=1.4, ls="--", zorder=4)
    ax.text(1.5, 3.0e9, "±3 ADU² of f32 error —\nit swallows the quiet pixels",
            color=ACCENT, fontsize=10.6, ha="center", va="center",
            linespacing=1.4)
    ax.annotate("", xy=(3.34, 3.4), xytext=(3.34, 6e7),
                arrowprops=dict(arrowstyle="->", color=ACCENT, lw=0.8))
    bare(ax)
    ax.set_title("var = E[X²] − mean²", color=MUTED, fontsize=11.2, pad=8, loc="left")

    d = json.loads((Path(__file__).resolve().parent
                    / "validation_tiers.json").read_text())
    e = np.array(d["edges"])
    ctr = 0.5 * (e[1:] + e[:-1])
    bw = e[1] - e[0]
    good = np.array(d["hists"]["cpu"], dtype=float)

    # +28.06 % more clusters (SS1), smeared upward from ~0 by a gate that has
    # collapsed to 0 sigma. Exponential with a 400 ADU scale: shape schematic,
    # integral set to the measured excess.
    LAM = 400.0
    excess = good.sum() * 0.2806 * (bw / LAM) * np.exp(-np.clip(ctr, 0, None) / LAM)
    ax2.step(ctr, good, where="mid", color=ACCENT, lw=1.8, zorder=4,
             label="f64 pedestal  ·  measured")
    ax2.step(ctr, good + excess, where="mid", color=AMBER, lw=1.5, zorder=3,
             label="f32 pedestal  ·  +28.06 %")
    ax2.fill_between(ctr, good, good + excess, step="mid", color=AMBER,
                     alpha=0.22, lw=0, zorder=2)
    ax2.set_yscale("log")
    ax2.set_ylim(2e2, 2e9)
    ax2.set_xlim(0, 2600)
    ax2.set_xticks([0, 1000, 2000])
    ax2.tick_params(labelsize=10.6, colors=MUTED)
    ax2.set_xlabel("cluster energy [ADU]", color=TEXT2, fontsize=11.2)
    bare(ax2, keep=("left", "bottom"))
    ax2.legend(frameon=False, fontsize=10.6, labelcolor=TEXT2, loc="lower right",
               handlelength=1.2, borderaxespad=0.3)
    # Both labels live in the decade of headroom above the 1.9e6 peak, so
    # neither can land on the curve at any zoom.
    ax2.annotate("clusters below the 5σ cut\nthat should not be there",
                 xy=(170, 3.0e4), xytext=(700, 2.4e8), color=AMBER, fontsize=10.6,
                 linespacing=1.4, zorder=6, ha="center",
                 arrowprops=dict(arrowstyle="->", color=AMBER, lw=0.9))
    ax2.text(1188, 6.0e6, "Cu Kα", color=PALE, fontsize=10.6, ha="center",
             va="bottom")
    ax2.set_title("cluster-energy spectrum, 3×3", color=MUTED, fontsize=11.2,
                  pad=8, loc="left")
    fig.subplots_adjust(left=0.075, right=0.995, top=0.86, bottom=0.20, wspace=0.24)
    save(fig, "fig_cancellation")


def fig_varfloor():
    """The quantitative version of the left panel: which pixels the error reaches.

    Floor is +-3-4 ADU^2 (docs/pedestal_precision_f32_cancellation.md SS5), so
    variance is LOST below rms ~2 and merely corrupted up to rms ~5. An earlier
    version drew the floor at 42 with the damage running to rms 6.5 -- that is
    6.5^2, i.e. the line and the shading were each other's source, and both were
    ~14x too large.
    """
    fig, ax2 = plt.subplots(figsize=(4.35, 3.05))
    rms = np.linspace(0, 6, 300)
    ax2.axvspan(0, 2.0, color=AMBER, alpha=0.20, zorder=1)
    ax2.axvspan(2.0, 5.0, color=AMBER, alpha=0.07, zorder=1)
    ax2.fill_between([0, 6], 3, 4, color=ACCENT, alpha=0.22, zorder=2, lw=0)
    ax2.plot(rms, rms**2, color=PALE, lw=1.8, zorder=4)
    ax2.axhline(3, color=ACCENT, lw=1.4, ls="--", zorder=5)
    pts = ((2.0, "rms 2 → var 4 ± 3 → 0", 0.30, 11.2),
           (3.0, "rms 3 → 17 % rms err", 0.30, 17.4),
           (5.0, "rms 5 → 6 % rms err",  2.30, 24.6))
    for r, lab, tx, ty in pts:
        ax2.plot([r], [r * r], marker="o", ms=4, color=PALE, zorder=6)
        ax2.annotate(lab, xy=(r, r * r), xytext=(tx, ty), color=TEXT2,
                     fontsize=11.2, va="center", zorder=6,
                     arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.6,
                                     shrinkA=2, shrinkB=3))
    ax2.set_xlabel("pixel rms (ADU)", color=TEXT2, fontsize=11.2)
    ax2.set_ylabel("variance (ADU²)", color=TEXT2, fontsize=11.2)
    ax2.set_ylim(0, 40); ax2.set_xlim(0, 6)
    ax2.set_yticks([0, 10, 20, 30, 40]); ax2.set_xticks([0, 2, 4, 6])
    ax2.tick_params(labelsize=10.4, colors=MUTED)
    bare(ax2, keep=("left", "bottom"))
    ax2.text(0.12, 38.8, "rms < 2\n→ clamped to 0\n→ fires every frame",
             color=AMBER, fontsize=11.2, va="top", linespacing=1.45, zorder=6)
    ax2.text(2.20, 38.8, "rms 2–5: threshold\ncorrupted, not clamped",
             color=MUTED, fontsize=11.2, va="top", linespacing=1.45, zorder=6)
    ax2.text(3.05, 31.0, "true variance = rms²", color=PALE, fontsize=11.2, zorder=6)
    ax2.text(5.90, 5.6, "f32 error floor  ±3–4 ADU²", color=ACCENT, fontsize=11.2,
             ha="right", zorder=6)
    fig.subplots_adjust(left=0.13, right=0.98, top=0.97, bottom=0.15)
    save(fig, "fig_varfloor")


fig_varfloor()


# ------------------------------- 10. the same kernel, measured five ways
def fig_bottleneck():
    """Why Act III comes last -- and why it is not even MEASURABLE before Act II.

    The quantity is the end-to-end change from the identical -40 % kernel, measured
    through each surviving step's result path at 9x9 (route A is annex-only). Each
    step was run 5 times per arm; the bar is the point estimate from best-of-warm
    and the whisker is what the two arms' own rep spreads allow.

    Through collect_view() the interval is 0.0 points wide. Through every allocating
    path it is 19-41 points wide and straddles zero: the result path does not merely
    shrink the kernel win, it destroys the ability to observe it at all. That is a
    stronger statement than the point estimates were, and unlike them it is robust.
    """
    fig, ax = plt.subplots(figsize=(11.4, 3.0))
    steps = ["opt3\nno overlap", "opt4\npinned",
             "opt5\nhost overlap", "opt6\nzero-copy"]
    pt = [16.0, -5.8, -6.8, -16.2]
    lo = [1.6, -17.2, -23.9, -16.2]
    hi = [20.3, 11.9, 17.2, -16.2]
    cols = [MUTED, MUTED, MUTED, AMBER]
    x = np.arange(len(steps))

    ax.axhline(0, color=RULE, lw=1.2, zorder=1)
    for xi, (p_, l_, h_, c) in enumerate(zip(pt, lo, hi, cols)):
        ax.plot([xi, xi], [l_, h_], color=c, lw=7, alpha=0.30,
                solid_capstyle="butt", zorder=2)
        ax.plot([xi - 0.13, xi + 0.13], [p_, p_], color=c, lw=2.6, zorder=3)
        if h_ - l_ < 1:
            ax.text(xi, p_ - 5.0, f"{p_:.1f}%", ha="center", color=PALE,
                    fontsize=13.6, fontweight="bold")
            ax.text(xi, p_ + 2.0, "resolvable to 0.0 pts", ha="center",
                    color=AMBER, fontsize=9.4)
        else:
            ax.text(xi, h_ + 1.6, f"{l_:+.0f} … {h_:+.0f}%", ha="center",
                    color=TEXT2, fontsize=11.2)

    ax.set_xticks(x); ax.set_xticklabels(steps, color=TEXT2, fontsize=10.6)
    ax.set_xlim(-0.6, 3.6)
    ax.set_ylim(-34, 30)
    ax.set_yticks([-20, 0, 20])
    ax.set_yticklabels(["−20 %", "0", "+20 %"], fontsize=9.4)
    bare(ax, keep=("left", "bottom"))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("end-to-end change from the SAME −40 % kernel, 9×9   ·   "
                 "bar = measurement, band = what the reps allow",
                 color=MUTED, fontsize=10.6, pad=10, loc="left")
    ax.text(1.5, -29.5, "through an allocating result path the effect is not measurable "
            "— the band straddles zero", color=MUTED, fontsize=10.0,
            ha="center", va="center")
    fig.subplots_adjust(bottom=0.24)
    save(fig, "fig_bottleneck")


# ---------------------------------------------------------- 11. correctness
def fig_correctness():
    fig, ax = plt.subplots(figsize=(7.4, 2.05))
    names = ["CPU MT", "opt1", "opt2", "opt3–opt6\n(f64)", "opt7\n(f32)"]
    diff = [0.0, 0.0041, 0.0039, 0.0039, 0.0039]
    colors = [MUTED, ACCENT, ACCENT, PALE, AMBER]
    x = np.arange(len(names))
    ax.bar(x, diff, width=0.5, color=colors, zorder=3)
    for xi, d in enumerate(diff):
        ax.text(xi, d + 0.00022, ("reference" if d == 0 else f"{d:.4f}%"),
                ha="center", color=PALE if d else MUTED, fontsize=10.0)
    ax.axhline(0.01, color=PALE, lw=1.2, ls="--")
    ax.text(4.4, 0.0104, "0.01% — well inside statistical noise", color=PALE,
            fontsize=9.4, ha="right")
    ax.set_xticks(x); ax.set_xticklabels(names, color=TEXT2, fontsize=10.0)
    ax.set_ylim(0, 0.0125); ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.set_title("cluster-count difference vs CPU  ·  233 M clusters, 3×3",
                 color=MUTED, fontsize=10.0, pad=8)
    save(fig, "fig_correctness")


# fig_bottleneck and fig_correctness are placed on no slide and have not been
# since the 22/34 renumber; fig_variance_rewrite joined them when slide 21 moved
# to A5, where the code block says the same thing in less space. The functions
# are kept -- they are the only record of how those figures were built -- but
# they are no longer called, so they stop leaving stale PNGs in docs/figures.
for f in (fig_arc, fig_arc_9x9, fig_first_run, fig_overhead, fig_streams, fig_pinning,
          fig_gpu_model,
          fig_graphs, fig_resultpath, fig_f32_kernel, fig_cancellation):
    f()
print("done ->", OUT)


# ------------------------------------------------- 12. opt5: host/GPU overlap
def fig_overlap():
    """What `submit(i+1)` before `collect(i)` actually does to the timeline.

    Two lanes, GPU and HOST, drawn for the same four chunks under both schedules.
    Serial: the host may only start chunk i once chunk i has come back, so the
    lanes never coexist and the frame costs GPU + host. Pipelined: chunk i+1 is
    submitted before chunk i is collected, so the lanes run together and the
    frame costs max(GPU, host) -- the saving is min(GPU, host), which is why it
    pays most when the two terms are comparable and least when one dominates.
    """
    fig, ax = plt.subplots(figsize=(11.6, 3.5))
    G, H = 2.6, 1.7          # chunk durations, arbitrary but 3x3-like proportions
    n = 4
    lane_h = 0.42

    def block(x, y, w, col, txt, tcol=BG):
        ax.add_patch(Rectangle((x, y), w, lane_h, facecolor=col, edgecolor=BG,
                               linewidth=1.4, zorder=3))
        ax.text(x + w / 2, y + lane_h / 2, txt, ha="center", va="center",
                color=tcol, fontsize=12.4, fontweight="bold", zorder=4)

    # ---- serial: G H G H G H G H, strictly alternating ----------------------
    yG, yH = 3.30, 2.78
    t = 0.0
    for i in range(n):
        block(t, yG, G, ACCENT, f"GPU {i+1}")
        t += G
        block(t, yH, H, PALE, f"host {i+1}")
        t += H
    serial_end = t

    # ---- pipelined: GPU back-to-back, host one chunk behind ----------------
    yG2, yH2 = 1.35, 0.83
    for i in range(n):
        block(i * G, yG2, G, ACCENT, f"GPU {i+1}")
    for i in range(n):
        block(G + i * G, yH2, H, PALE, f"host {i+1}")
    pipe_end = G * n + H

    for y, lbl in ((yG, "GPU"), (yH, "host"), (yG2, "GPU"), (yH2, "host")):
        ax.text(-0.25, y + lane_h / 2, lbl, ha="right", va="center",
                color=TEXT2, fontsize=12.4)

    ax.text(-0.25, yG + lane_h + 0.30, "submit → collect, serialized   (opt4)   ·   3×3",
            ha="left", va="bottom", color=TEXT2, fontsize=12.4, fontweight="bold")
    ax.text(-0.25, yG2 + lane_h + 0.30,
            "submit(i+1) before collect(i)   (opt5)",
            ha="left", va="bottom", color=AMBER, fontsize=12.4, fontweight="bold")

    for x, y0, y1, col in ((serial_end, yH, yG + lane_h, MUTED),
                           (pipe_end, yH2, yG2 + lane_h, AMBER)):
        ax.plot([x, x], [y0 - 0.18, y1 + 0.18], color=col, lw=1.2, ls="--",
                zorder=5)

    ax.annotate("", xy=(pipe_end, 0.42), xytext=(serial_end, 0.42),
                arrowprops=dict(arrowstyle="<|-|>", color=AMBER, lw=1.5))
    ax.text((pipe_end + serial_end) / 2, 0.20,
            "saved: min(GPU, host) per chunk", ha="center", va="top",
            color=AMBER, fontsize=12.4, fontweight="bold")

    ax.text(serial_end + 0.25, yG + lane_h / 2, "GPU + host  per chunk",
            ha="left", va="center", color=MUTED, fontsize=12.4)
    ax.text(pipe_end + 0.25, yG2 + lane_h / 2, "max(GPU, host)  per chunk",
            ha="left", va="center", color=AMBER, fontsize=12.4, fontweight="bold")

    ax.set_xlim(-1.6, serial_end + 4.0)
    ax.set_ylim(0, 4.25)
    ax.axis("off")
    save(fig, "fig_overlap")


# ----------------------------- 12b. the 9x9 case: overlap runs out (opt5->opt6)
def fig_overlap_9x9():
    """Why opt5 stops at 9x9, and why a deeper buffer cannot restart it.

    Same pipelined loop as fig_overlap, but with the measured 9x9 proportions:
    the GPU delivers a chunk every 30.01 us and the host needs ~62 (the
    fault-corrected steady-state term; see the slide's caption). The host lane is
    therefore the packed one, and it alone sets the finish line.

    The second strip is the answer to "add more slots". With three, the GPU front-
    loads instead of stalling between chunks 2 and 3 -- but the host lane is
    IDENTICAL in both strips, because it is already saturated, so both finish at
    exactly the same time. A deeper buffer relocates GPU idle; it does not remove
    it, and it cannot speed up the stage that is binding.
    """
    fig, ax = plt.subplots(figsize=(11.2, 3.5))
    G, H = 30.0, 62.0
    lane_h = 8.0
    n = 4

    def block(x, y, w, col, txt):
        ax.add_patch(Rectangle((x, y), w, lane_h, facecolor=col, edgecolor=BG,
                               linewidth=1.4, zorder=3))
        ax.text(x + w / 2, y + lane_h / 2, txt, ha="center", va="center",
                color=BG, fontsize=11.8, fontweight="bold", zorder=4)

    # The host is saturated in both cases, so its lane is the same schedule twice:
    # chunk i is collected as soon as the host is free, never before G.
    host_start = [G + i * H for i in range(n)]
    finish = host_start[-1] + H

    # 2 slots: the GPU may only run one chunk ahead, so it waits for a slot to free
    gpu2, free_at = [], [0.0, 0.0]
    t = 0.0
    for i in range(n):
        t = max(t, free_at[i % 2])
        gpu2.append(t); t += G
        free_at[i % 2] = host_start[i] + H      # slot returns when the host is done
    # 3 slots: one more chunk of runway before the same wall
    gpu3, free_at3 = [], [0.0, 0.0, 0.0]
    t = 0.0
    for i in range(n):
        t = max(t, free_at3[i % 3])
        gpu3.append(t); t += G
        free_at3[i % 3] = host_start[i] + H

    for row, (gpu, tag, col) in enumerate([
            (gpu2, "2 slots  ·  what ships", AMBER),
            (gpu3, "3 slots  ·  the natural next guess", MUTED)]):
        # Row spacing has to clear the row's TALLEST element, which is the tag
        # sitting above its GPU lane -- not the lane itself. 37 leaves ~0.45 in
        # between one row's tag and the row above's host blocks.
        yG = 55.0 - row * 40.0
        yH = yG - 12.0
        for i in range(n):
            block(gpu[i], yG, G, ACCENT, f"GPU {i + 1}")
            block(host_start[i], yH, H, PALE, f"host {i + 1}")
        for lbl, y in (("GPU", yG), ("host", yH)):
            ax.text(-6, y + lane_h / 2, lbl, ha="right", va="center",
                    color=TEXT2, fontsize=12.4)
        # The tag is left-aligned and 70 x-units long; the idle marker sits at
        # the middle of the stall, which is inside that span. They can only be
        # separated vertically, so the tag clears the marker by a full line.
        ax.text(-6, yG + lane_h + 13.0, tag, ha="left", va="bottom",
                color=col, fontsize=12.4, fontweight="bold")
        # every gap the GPU sits through, marked where it happens
        for i in range(1, n):
            gap = gpu[i] - (gpu[i - 1] + G)
            if gap > 4.0:   # a 2 us seam is not an argument, only a real stall is
                ax.annotate("", xy=(gpu[i], yG + lane_h / 2),
                            xytext=(gpu[i - 1] + G, yG + lane_h / 2),
                            arrowprops=dict(arrowstyle="<|-|>", color=AMBER, lw=1.3))
                ax.text((gpu[i] + gpu[i - 1] + G) / 2, yG + lane_h + 1.0,
                        f"idle {gap:.0f} µs", ha="center", va="bottom",
                        color=AMBER, fontsize=12.4, fontweight="bold")

    ax.plot([finish, finish], [1, 70], color=GREEN, lw=1.6, ls="--", zorder=6)
    ax.text(finish + 5, 33, "same finish\nboth ways", ha="left", va="center",
            color=GREEN, fontsize=13.0, fontweight="bold")

    ax.set_xlim(-32, finish + 62)
    ax.set_ylim(0, 86)
    ax.axis("off")
    save(fig, "fig_overlap_9x9")

# --------------------------- 12c. WHICH test carries the serial-vs-frozen gap
def fig_test3():
    """Measured proof that Test3 is the channel, and Test1 is not.

    The two panels are deliberately on DIFFERENT clocks, and each says so:

    Left is the 21x21 neighbourhood of the site AFTER the frame is finished --
    the branch map is read once find_clusters() returns, so every cell carries a
    final decision. An earlier version drew a "the scan is here" cursor over it,
    which was a contradiction: the map has no undecided cells to point at. The
    arrows now run uniformly across every row and mean raster ORDER, not
    progress.

    Right is the instant the centre pixel was tested, which is the only moment at
    which the two models can be said to disagree. Four of its neighbours are in
    the raster's past and may already carry this frame's push; four are in its
    future and cannot. Exactly those four differ, and the centre does not.

    Three fills, because the finder has three outcomes and merging any two of
    them invites the question "why is that photon 3x4 pixels?":

        stored   the window max, and it clears 5 sigma        (~1.5 %)
        shadow   the window max clears 5 sigma, but this      (~18 %)
                 pixel is not it -- and note the pixel
                 itself need NOT be bright: a 17.7 ADU
                 pixel is shadow if its 3x3 reaches the
                 345 ADU one next door
        sample   nothing in the window clears; push pedestal  (~80 %)

    So a shadow region is not a photon's 3x3. It is the union of every window
    that can SEE something above 5 sigma, which is why sharing charge across two
    adjacent pixels (810.3 and 345.0 here, against a bar of 85.5) lights up 3x4.

    The site itself is marked amber-filled with a green ring, because the panel
    is coloured by the SERIAL finder and serial pushed the pedestal there. Only
    frozen stored. That disagreement is the whole slide.

    The punchline is in the numbers, not the picture: `max` is IDENTICAL in the
    two models (56.473), so Test1 provably did not move. Only the SUM crossed.
    """
    site = json.loads((Path(__file__).resolve().parent / "branch_site.json").read_text())
    vf = np.array(site["val_frz"]); vc = np.array(site["val_cpu"])
    scanned = np.array(site["scanned"], dtype=bool)
    thr = site["thr_test3_frz"]
    pat = site["patch"]
    bc = np.array(pat["branch_cpu"]); R = pat["r"]

    fig, (ax, bx) = plt.subplots(1, 2, figsize=(12.4, 4.25),
                                 gridspec_kw=dict(width_ratios=[1.28, 1.0]))
    # An equal-aspect axes shrinks its BOX to the drawing and then centres it,
    # which parked both grids in the middle of their slots with dead space on
    # either side. Anchor west so each grid sits over the legend that decodes it.
    ax.set_anchor("W"); bx.set_anchor("W")

    # ---- left: the finished frame, 21x21 around the site -------------------
    # Colour by what the SERIAL finder decided: the pedestal-sample cells are
    # exactly the ones a later stencil can read differently from the frozen
    # model, which is the mechanism this slide is about. Shadow gets its own
    # fill -- welding it to `stored` is what made a photon look 3x4 wide.
    SHADOW = "#33455E"
    FILL = {4: AMBER, 1: SHADOW, 2: GREEN, 3: GREEN, 6: SHADOW,
            0: SHADOW, 5: PANEL}
    n = 2 * R + 1
    for r in range(n):
        for c in range(n):
            ax.add_patch(Rectangle((c, n - 1 - r), 1, 1,
                                   facecolor=FILL.get(int(bc[r, c]), PANEL),
                                   edgecolor=BG, linewidth=0.5, zorder=2))
    # Raster ORDER, not raster progress, and drawn OUTSIDE the data: arrows laid
    # over the cells vanished against the amber and re-introduced the "we are
    # here" reading. Two margin arrows say left-to-right, then top-to-bottom.
    ax.annotate("", xy=(n, n + 0.55), xytext=(0.0, n + 0.55),
                arrowprops=dict(arrowstyle="-|>", color=MUTED, lw=1.2))
    ax.annotate("", xy=(-0.85, 0.0), xytext=(-0.85, n),
                arrowprops=dict(arrowstyle="-|>", color=MUTED, lw=1.2))
    ax.text(n / 2, n + 0.85, "raster order", ha="center", va="bottom",
            color=MUTED, fontsize=12.4)
    # the window Test3 summed, and inside it the one pixel the models disagree on
    ax.add_patch(Rectangle((R - 1, n - R - 2), 3, 3, facecolor="none",
                           edgecolor=PALE, linewidth=1.4, ls=(0, (3, 2)),
                           zorder=6))
    ax.add_patch(Rectangle((R, n - R - 1), 1, 1, facecolor="none",
                           edgecolor=GREEN, linewidth=2.4, zorder=7))
    ax.annotate("the pixel they\ndisagree on", xy=(R + 1.6, n - R - 0.5),
                xytext=(n + 1.4, n - R - 0.5), ha="left", va="center",
                color=PALE, fontsize=12.4,
                arrowprops=dict(arrowstyle="-", color=PALE, lw=1.0,
                                shrinkA=2, shrinkB=2))
    # Headroom for the title ABOVE the raster-order label, not on top of it.
    ax.set_xlim(-1.6, n + 8.8); ax.set_ylim(-0.4, n + 3.6)
    ax.set_aspect("equal"); ax.axis("off")
    ax.text(n / 2, n + 3.5, f"the finished frame · 21 × 21 around "
                            f"({site['iy']}, {site['ix']})",
            ha="center", va="top", color=TEXT2, fontsize=12.4)

    # ---- right: the 3x3, with each finder's reading named -------------------
    for r in range(3):
        for c in range(3):
            past = scanned[r, c]
            bx.add_patch(Rectangle((c, 2 - r), 1, 1,
                                   facecolor="#1B2534" if past else PANEL,
                                   edgecolor=AMBER if past else BG,
                                   linewidth=2.2 if past else 1.6, zorder=2))
            if past:
                bx.text(c + 0.5, 2 - r + 0.66, f"{vf[r, c]:.3f}", ha="center",
                        va="center", color=RED, fontsize=13.0,
                        fontweight="bold", zorder=3)
                bx.text(c + 0.5, 2 - r + 0.32, f"{vc[r, c]:.3f}", ha="center",
                        va="center", color=ACCENT, fontsize=13.0,
                        fontweight="bold", zorder=3)
            else:
                bx.text(c + 0.5, 2 - r + 0.50, f"{vf[r, c]:.3f}", ha="center",
                        va="center", color=PALE, fontsize=13.0,
                        fontweight="bold", zorder=3)
    bx.add_patch(Rectangle((1, 1), 1, 1, facecolor="none", edgecolor=GREEN,
                           linewidth=2.6, zorder=4))
    bx.set_xlim(-0.06, 3.06); bx.set_ylim(-0.06, 3.66)
    bx.set_aspect("equal"); bx.axis("off")
    bx.text(1.5, 3.60, f"the moment ({site['iy']}, {site['ix']}) was tested · "
                       f"only the 4 already-scanned neighbours differ",
            ha="center", va="top", color=TEXT2, fontsize=12.4)

    # The verdict goes in figure coords: inside the axes it would be laid out
    # against an equal-aspect box and squeeze the grid into a column. Three
    # fills, three legend rows -- a swatch each, because the shadow fill is far
    # too dark to identify from coloured text.
    def key(y, col, txt, edge=None):
        fig.add_artist(Rectangle((0.020, y - 0.004), 0.017, 0.042,
                                 facecolor=col, edgecolor=edge or BG,
                                 linewidth=2.0 if edge else 0.8,
                                 transform=fig.transFigure))
        fig.text(0.046, y + 0.017, txt, color=TEXT2, fontsize=12.4,
                 va="center")

    key(0.245, GREEN, "stored: this pixel IS the window max, and it clears 5σ")
    key(0.170, SHADOW, "shadow: the window max clears 5σ, but this pixel is not it")
    key(0.095, AMBER, "pedestal sample: nothing in the window clears 5σ")
    key(0.020, AMBER, "the disputed pixel — serial sampled here, frozen stored",
        edge=GREEN)

    # No hand-placed colour key: the two sigma rows below are already
    # colour-coded, so the cells decode from them.
    # Keep this line no longer than the sigma rows below it: bbox_inches="tight"
    # widens the canvas to the widest string, which shrinks every other one.
    fig.text(0.545, 0.250, "frozen above serial · amber ring = pushed pedestal",
             color=TEXT2, fontsize=12.4)
    fig.text(0.545, 0.175, f"serial   Σ = {site['total_cpu']:.3f}   <   "
                           f"{thr:.3f}   →  pedestal sample",
             color=ACCENT, fontsize=12.4, fontweight="bold")
    fig.text(0.545, 0.100, f"frozen  Σ = {site['total_frz']:.3f}   >   "
                           f"{thr:.3f}   →  CLUSTER",
             color=RED, fontsize=12.4, fontweight="bold")
    fig.text(0.545, 0.025, f"max = {site['max_frz']:.3f} in both   →  "
                           f"Test1 never moved",
             color=GREEN, fontsize=12.4, fontweight="bold")

    fig.subplots_adjust(left=0.015, right=0.985, top=0.97, bottom=0.31,
                        wspace=0.06)
    save(fig, "fig_test3")

# ----------------------------- 13b. what a minor page fault actually costs
def fig_pagefault():
    """Why the first run is slow, drawn once so the slide can stop explaining it.

    Two ideas the room may not share: malloc hands back VIRTUAL pages, and a
    virtual page has no physical frame behind it until something touches it. The
    cost is not the lookup, it is that the kernel must ZERO 4 kB before it can
    hand the frame over -- mandatory, because the frame last belonged to someone
    else. Nothing here is CUDA-specific, which is the point: the benchmark was
    measuring Linux.

    Every label lives outside the two rows. An earlier version put "already
    mapped" and "you write here" between them, where they collided with the very
    arrows they were naming.
    """
    fig, ax = plt.subplots(figsize=(11.4, 2.85))
    PW, GAP, N = 9.0, 1.6, 4
    xs = [2 + i * (PW + GAP) for i in range(N)]
    cx = [x + PW / 2 for x in xs]

    def box(x, y, col, dashed):
        ax.add_patch(Rectangle((x, y), PW, 4.6,
                               facecolor=BG if dashed else PANEL, edgecolor=col,
                               linewidth=1.5,
                               linestyle=(0, (2, 2)) if dashed else "-", zorder=3))

    for i, x in enumerate(xs):
        box(x, 32.0, RED if i == 2 else ACCENT, False)
        box(x, 9.0, [AMBER, AMBER, RED, MUTED][i], i >= 2)
    ax.text(2, 38.6, "VIRTUAL  ·  the ClusterVector malloc just handed you",
            ha="left", va="bottom", color=TEXT2, fontsize=12.4)
    ax.text(2, 7.8, "PHYSICAL  ·  4 kB frames the kernel actually owns",
            ha="left", va="top", color=TEXT2, fontsize=12.4)

    for i in (0, 1):
        ax.annotate("", xy=(cx[i], 13.8), xytext=(cx[i], 31.9),
                    arrowprops=dict(arrowstyle="-|>", color=GREEN, lw=1.5))
    ax.annotate("", xy=(cx[2], 13.8), xytext=(cx[2], 31.9),
                arrowprops=dict(arrowstyle="-|>", color=RED, lw=1.8,
                                linestyle=(0, (3, 2))))

    # Key UNDER the two rows, not beside them: the right-hand half of the figure
    # belongs to the fault panel, and "first touch → fault" is 18 x-units wide,
    # which is exactly the clearance that was left between the two.
    for x0, col, ls, lab in ((2, GREEN, "-", "already mapped"),
                             (26, RED, (0, (3, 2)), "first touch → fault")):
        ax.plot([x0, x0 + 4.5], [2.4, 2.4], color=col, lw=1.8, ls=ls)
        ax.text(x0 + 5.6, 2.4, lab, ha="left", va="center", color=col,
                fontsize=12.4)

    ax.add_patch(FancyBboxPatch((58, 4), 42, 34,
                                boxstyle="round,pad=0,rounding_size=1.2",
                                facecolor=PANEL, edgecolor=RED, linewidth=1.5,
                                zorder=3))
    ax.text(60.5, 34.4, "MINOR FAULT  ·  in the kernel", ha="left",
            va="center", color=RED, fontsize=12.4, fontweight="bold")
    for i, (n, txt, hot) in enumerate([("1", "traps into the kernel", False),
                                       ("2", "finds a free frame", False),
                                       ("3", "zeroes all 4 kB of it", True),
                                       ("4", "maps it; the write proceeds", False)]):
        y = 28.8 - i * 5.2
        ax.text(61.0, y, n, ha="left", va="center", color=MUTED, fontsize=12.4)
        ax.text(64.0, y, txt, ha="left", va="center",
                color=AMBER if hot else TEXT2, fontsize=12.4,
                fontweight="bold" if hot else "normal")
    ax.text(60.5, 6.6, "no disk: that would be a MAJOR fault", ha="left",
            va="center", color=MUTED, fontsize=12.4)

    ax.set_xlim(-1, 101); ax.set_ylim(0, 42)
    ax.axis("off")
    save(fig, "fig_pagefault")


fig_pagefault()   # defined after the run list above, so called here


# ------------------------------------- 13. pedestal update timing, three ways
def fig_pedtiming():
    """Why ClusterFinderFrozen exists: the one variable it holds still.

    Within a single frame the serial CPU finder updates the pedestal AS the raster
    scan passes each pixel, so a decision late in the frame is taken against a
    pedestal that already contains this frame's earlier pixels. Frozen and CUDA
    both decide against the frame-start snapshot and apply every update at the
    frame boundary.

    That makes the comparison factorable: cpu-vs-frozen isolates update TIMING with
    the arithmetic held fixed, and frozen-vs-cuda isolates the PORT with the timing
    held fixed. Comparing cuda straight to the serial CPU confounds the two.
    """
    fig, ax = plt.subplots(figsize=(11.6, 3.6))
    XF, XE = 6.0, 7.4               # frame end, dotted continuation end
    rows = [("ClusterFinder\nserial CPU", 3.00, ACCENT, True),
            ("ClusterFinderFrozen\nCPU twin", 1.70, PALE, False),
            ("ClusterFinderCUDA", 0.40, AMBER, False)]

    for label, y, col, stair in rows:
        ax.text(-0.35, y + 0.26, label, ha="right", va="center", color=TEXT2,
                fontsize=11.2, linespacing=1.35)
        ax.plot([0, XE], [y, y], color=RULE, lw=1.0, zorder=1)

        if stair:
            xs = np.linspace(0, XF, 22)
            ys = y + 0.10 + 0.42 * xs / XF
            ax.step(xs, ys, where="post", color=col, lw=2.2, zorder=3)
            ax.plot([XF, XE], [y + 0.52, y + 0.62], color=col, lw=2.0, ls=":",
                    zorder=3)
            ax.text(XF / 2, y + 0.64, "the pedestal moves DURING the scan",
                    ha="center", va="bottom", color=col, fontsize=10.6,
                    fontweight="bold")
            ax.text(XF / 2, y - 0.13,
                    "a late pixel is judged against a pedestal that already moved",
                    ha="center", va="top", color=MUTED, fontsize=10.0)
        else:
            ax.plot([0, XF], [y + 0.10] * 2, color=col, lw=2.2, zorder=3)
            ax.plot([XF, XF], [y + 0.10, y + 0.52], color=col, lw=2.2, zorder=3)
            ax.plot([XF, XE], [y + 0.52] * 2, color=col, lw=2.0, ls=":", zorder=3)
            # ABOVE the riser, not on the trace: at y + 0.18 the label sat
            # directly on the 2.2 pt line it was labelling.
            ax.text(XF / 2, y + 0.62, "every decision uses the frame-start snapshot",
                    ha="center", va="bottom", color=col, fontsize=10.6,
                    fontweight="bold")

    ax.axvline(XF, color=MUTED, lw=1.1, ls="--", zorder=2, ymin=0.02, ymax=0.93)
    ax.text(XF / 2, 4.10, "one frame  ·  160 000 pixels in raster order",
            ha="center", va="bottom", color=MUTED, fontsize=11.2)
    ax.text(XF + 0.12, 4.10, "frame ends →\nupdates applied", ha="left",
            va="bottom", color=MUTED, fontsize=10.0, linespacing=1.35)

    # what the middle row buys: two comparisons, one variable each
    XA = 8.35
    ax.annotate("", xy=(XA, 1.87), xytext=(XA, 3.17),
                arrowprops=dict(arrowstyle="<|-|>", color=PALE, lw=1.5))
    ax.text(XA + 0.18, 2.52, "update TIMING\narithmetic held fixed", ha="left",
            va="center", color=PALE, fontsize=10.6, fontweight="bold",
            linespacing=1.4)
    ax.annotate("", xy=(XA, 0.57), xytext=(XA, 1.87),
                arrowprops=dict(arrowstyle="<|-|>", color=AMBER, lw=1.5))
    ax.text(XA + 0.18, 1.22, "the PORT\ntiming held fixed", ha="left",
            va="center", color=AMBER, fontsize=10.6, fontweight="bold",
            linespacing=1.4)

    ax.set_xlim(-2.9, 12.0)
    ax.set_ylim(-0.30, 4.60)
    ax.axis("off")
    save(fig, "fig_pedtiming")


fig_overlap()
fig_overlap_9x9()
fig_test3()
fig_pedtiming()


# --------------------------------------- 14. frame 147, frozen | cuda, masked
def fig_mismatch147():
    """The mismatch as the notebook shows it: two masked frames, side by side.

    Only the 3x3 footprints of each finder's OWN centres are drawn; everything
    else is blank, so the panels differ exactly where the finders do. Values are
    pedestal-subtracted ADU under that finder's decision-time pedestal, and the
    amber ring marks the one centre cuda keeps that frozen does not.

    Data: frame147.json, written by scratchpad/dump147.py.
    """
    import json
    d = json.loads((Path(__file__).resolve().parent
                    / "frame147.json").read_text())
    X0, Y0 = d["x0"], d["y0"]
    only = {tuple(p) for p in d["only_cuda"]}

    sub = {"frozen": np.array(d["sub_frozen"]), "cuda": np.array(d["sub_cuda"])}
    msk = {"frozen": np.array(d["mask_frozen"]), "cuda": np.array(d["mask_cuda"])}
    cen = {"frozen": d["c_frozen"], "cuda": d["c_cuda"]}

    union = msk["frozen"] | msk["cuda"]
    vmax = max(float(np.percentile(sub["frozen"][union], 99)), 50.0)

    cmap = plt.cm.viridis.copy()
    cmap.set_bad(PANEL)

    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.9))
    for ax, name, col in ((axes[0], "frozen", PALE), (axes[1], "cuda", ACCENT)):
        w, m = sub[name], msk[name]
        ax.imshow(np.ma.masked_where(~m, w), cmap=cmap, vmin=0, vmax=vmax,
                  interpolation="nearest", origin="upper")
        for i in range(w.shape[0]):
            for j in range(w.shape[1]):
                if m[i, j]:
                    # A cell is ~0.18 in wide in the saved figure and a value can
                    # be three digits, so anything above ~5.5 pt collides with
                    # the cell next to it. These are texture, not readings: the
                    # slide's argument is which pixels are lit, and the numbers
                    # are on annex A7 and in the notes for anyone who wants them.
                    ax.text(j, i, f"{w[i, j]:.0f}", ha="center", va="center",
                            fontsize=5.4, zorder=7, gid="texture",
                            color="white" if w[i, j] < 0.55 * vmax else "black")
        for (cx, cy) in cen[name]:
            ax.plot(cx - X0, cy - Y0, ".", color="#FF4B4B", ms=4.5, zorder=4)
        for (cx, cy) in only:
            if name == "cuda":
                ax.add_patch(plt.Circle((cx - X0, cy - Y0), 2.6, fill=False,
                                        ec=AMBER, lw=2.0, zorder=6))
            else:
                ax.add_patch(plt.Circle((cx - X0, cy - Y0), 2.6, fill=False,
                                        ec=AMBER, lw=1.4, ls=":", zorder=6))
        ax.set_title(f"{name}  —  {d['n_' + name]:,} clusters in this frame",
                     color=col, fontsize=11.8, fontweight="bold", pad=7)
        ax.set_xticks([]); ax.set_yticks([])
        for sp in ax.spines.values():
            sp.set_color(RULE)

    axes[0].text(-0.03, 0.5, f"frame {d['fid']}\nzoom on (202, 8)",
                 transform=axes[0].transAxes, rotation=90, ha="right",
                 va="center", color=MUTED, fontsize=10.6, linespacing=1.4)
    # anchored in DATA coordinates: the window is no longer square (the cut is
    # clipped by the top edge of the frame), so axes fractions do not track the
    # pixel once matplotlib letterboxes the image to keep aspect.
    (ox, oy), = only
    axes[1].annotate("cuda keeps this one;\nfrozen does not",
                     xy=(ox - X0 + 2.9, oy - Y0), xytext=(0.99, 0.93),
                     xycoords="data", textcoords="axes fraction",
                     color=AMBER, fontsize=10.6, fontweight="bold", ha="right",
                     linespacing=1.35,
                     arrowprops=dict(arrowstyle="-|>", color=AMBER, lw=1.3))
    fig.subplots_adjust(wspace=0.06)
    save(fig, "fig_mismatch147")


fig_mismatch147()


# ------------------------------------ 15. what fills an SM first, per cluster size
def fig_regpressure():
    """Occupancy is an OUTPUT. This is the input: what runs out first.

    One SM holds 65 536 registers and 1 536 thread slots. A 16x16 block is 256
    threads, so a block costs regs_per_thread x 256 registers and 256 slots. At
    3x3 the slots run out first and the register file still has room; at 9x9 the
    register file is exactly full at two blocks, which strands two thirds of the
    slots. Same kernel, same block size, opposite binding resource.
    """
    rows = [
        ("3×3", 38, 6, ACCENT),
        ("9×9", 128, 2, AMBER),
    ]
    fig, ax = plt.subplots(figsize=(7.4, 2.15))

    y, ticks, labels = 0.0, [], []
    for name, regs, blocks, col in rows:
        for kind, used, cap in (("register file", regs * 256 * blocks, 65536),
                                ("thread slots", 256 * blocks, 1536)):
            frac = 100.0 * used / cap
            seg = frac / blocks
            for b in range(blocks):                    # one segment per block
                ax.barh(y, seg - 0.5, left=b * seg + 0.25, height=0.52,
                        color=col, zorder=3, linewidth=0)
            full = abs(frac - 100.0) < 0.6
            ax.text(frac + 1.5, y, f"{frac:.0f} %", va="center", fontsize=11.8,
                    color=PALE if full else TEXT2,
                    fontweight="bold" if full else "normal")
            ticks.append(y); labels.append(f"{name}  ·  {kind}")
            y -= 1.0
        y -= 0.45

    ax.axvline(100, color=MUTED, lw=1.1, ls="--", zorder=4)
    ax.text(99, 1.05, "capacity of one SM", color=MUTED, fontsize=11.8,
            ha="right")
    ax.set_yticks(ticks)
    ax.set_yticklabels(labels, color=TEXT2, fontsize=11.8)
    ax.set_xlim(0, 172); ax.set_xticks([])
    ax.set_ylim(y + 0.6, 1.5)
    bare(ax, keep=("left",))
    ax.tick_params(axis="y", length=0)

    # Clear of the "100 %" value labels, which are bold and ~10 x-units wide.
    ax.text(122, ticks[0] - 0.5, "6 blocks resident\n38 × 256 = 9 728 regs each",
            color=ACCENT, fontsize=11.8, va="center", linespacing=1.4)
    ax.text(122, ticks[2] - 0.5, "2 blocks resident\n128 × 256 = 32 768 regs each",
            color=AMBER, fontsize=11.8, va="center", linespacing=1.4)
    fig.subplots_adjust(left=0.16, right=0.99, top=0.88, bottom=0.06)
    save(fig, "fig_regpressure")


fig_regpressure()


# ------------------------------------------ 4b. the timeline, revealed in steps
# fig_streams shows all three stages at once and belongs on the opt3 slide, where
# removing the barrier is the point. These two are the earlier beats of the same
# picture, so opt1 and opt2 can each show the state of play at their own step.

def _engine_legend(ax, y=0.92):
    handles = [Rectangle((0, 0), 1, 1, color=c) for c in (AMBER, ACCENT, PALE)]
    ax.legend(handles, ["H2D copy", "kernel", "D2H copy"], frameon=False,
              fontsize=11.2, labelcolor=TEXT2, ncol=3, loc="lower right",
              bbox_to_anchor=(1.02, y), handlelength=1.1)


def fig_opt1_timeline():
    """opt1 — one stream, synchronous: one engine at a time, host idle between.

    Proportions are opt1's own: 13.1 + 14.7 + 5.3 = 33.2 us of serialized engine
    time inside a 63.3 us frame, so the host gap is drawn ~48 % of the period. The
    contiguous-bars version of this picture (fig_streams, panel 1) implies the GPU
    is busy end to end, which is exactly what opt1 is not.
    """
    WORK = H_ + K_ + D_                 # 33 units = 33.2 us of engine work
    HOST = 30                           # 63.3 - 33.2 = 30.1 us the host holds
    PER = WORK + HOST
    fig, ax = plt.subplots(figsize=(7.7, 1.05))
    for i in range(3):
        t0 = i * PER
        _frame_bars(ax, 1.0, t0)
        ax.axvspan(t0 + WORK, t0 + PER, facecolor=AMBER, alpha=0.17, zorder=1)
        ax.axvline(t0 + WORK, color=AMBER, lw=0.9, alpha=0.55, zorder=1)
        ax.axvline(t0 + PER, color=AMBER, lw=0.9, alpha=0.55, zorder=1)
    ax.annotate("host blocks: every engine idle",
                xy=(WORK + HOST / 2, 0.98), xytext=(WORK + HOST / 2, 0.56),
                color=AMBER, fontsize=10.6, ha="center", va="top",
                arrowprops=dict(arrowstyle="-", color=AMBER, lw=0.8))
    ax.text(3 * PER + 4, 1.34, "one engine\nat a time", color=TEXT2, fontsize=10.6,
            va="center", linespacing=1.5)
    ax.set_xlim(-4, 3 * PER + 40)
    ax.set_ylim(0.02, 2.45)
    ax.set_yticks([]); ax.set_xticks([])
    bare(ax, keep=())
    _engine_legend(ax)
    fig.subplots_adjust(left=0.01, right=0.99, top=0.86, bottom=0.04)
    save(fig, "fig_opt1_timeline")


def fig_opt2_timeline():
    """opt2 — four streams, ONE round: what streaming buys, and nothing else.

    Scheduled by _schedule(), so the four H2D bars queue on the single copy
    engine instead of being drawn on top of each other. The stagger is therefore
    not a drawing choice: it is exactly H2D's duration, which is why the lanes
    step by 13 units. The barrier is opt3's subject and is left to that slide.
    """
    fig, ax = plt.subplots(figsize=(7.7, 1.50))
    frames, _ = _schedule(4, 4)
    _draw_schedule(ax, frames)
    for st in range(4):
        ax.text(-4, (3 - st) + LANE_ / 2, f"stream {st}", color=MUTED,
                fontsize=10.6, ha="right", va="center")
    # The window where the most frames are simultaneously in flight -- measured
    # off the schedule, not asserted. At 3x3 proportions it is THREE, not four:
    # the frame span (33) is only 2.5x the H2D stagger (13), so stream 0 has
    # already retired by the time stream 3 gets the copy engine. The old drawing
    # claimed all four, which the single copy engine makes impossible here.
    spans = [(h0, d0 + D_) for _, h0, _, d0 in frames]
    edges = sorted({t for sp in spans for t in sp})
    counts = [(a, b, sum(1 for s0, s1 in spans if s0 <= a and s1 >= b))
              for a, b in zip(edges, edges[1:])]
    best = max(c for _, _, c in counts)
    lo = min(a for a, _, c in counts if c == best)
    hi = max(b for _, b, c in counts if c == best)
    ax.axvspan(lo, hi, color=ACCENT, alpha=0.10, zorder=1)
    ax.text((lo + hi) / 2, 4.05, f"{best} frames in flight", color=ACCENT,
            fontsize=10.6, ha="center", va="bottom")
    ax.set_xlim(-26, 100)
    ax.set_ylim(-0.35, 4.6)
    ax.set_yticks([]); ax.set_xticks([])
    bare(ax, keep=())
    ax.text(74, 0.34, "a copy in one stream runs\nwhile another computes",
            color=TEXT2, fontsize=10.6, va="center", linespacing=1.5)
    _engine_legend(ax, y=0.94)
    ax.set_xlabel("time  →", color=MUTED, fontsize=10.6, loc="left")
    fig.subplots_adjust(left=0.01, right=0.99, top=0.90, bottom=0.19)
    save(fig, "fig_opt2_timeline")


fig_opt1_timeline()
fig_opt2_timeline()


# ------------------------------- 10b. the same typedef, the same absolute gain
def fig_f32_absolute():
    """The same typedef, the same ~4.7 us -- and a percentage that triples.

    9x9 end-to-end, f64 vs f32, warm, cap 1700, from ladder_9x9.csv in
    results/2026-08-20_{f64,f32}_cap1700/.

    opt3 is EXCLUDED, not dropped for space: its two arms differ by 393k page
    faults, which the deck's own 0.68 us/fault model turns into +13.4 us against
    an observed +13.2. That reading measured the allocator, not the kernel, and
    quoting it as "+16 %" would repeat the error this slide exists to expose.

    opt5 is shown but greyed: its arms differ by 141k faults (-4.81 us predicted
    against -4.54 observed), so the fault term alone accounts for the whole
    effect. opt4 (faults matched to 493) and opt6 (zero faults in both arms) are
    the two clean readings -- and they agree, -4.63 and -4.87 us.

    Two panels because one cannot carry it: a 4.6 us delta on an 80 us axis is
    invisible, which is itself the point. Left = the frame shrinking; right =
    the saving that does not.
    """
    steps = ["opt4", "opt5", "opt6"]
    sub = ["+ pinned input", "host↔GPU overlap", "zero-copy"]
    f64v = [79.83, 66.39, 30.01]
    f32v = [75.20, 61.85, 25.14]
    dv = [a - b for a, b in zip(f64v, f32v)]
    pct = [-5.8, -6.8, -16.2]
    dagger = [False, True, False]

    fig, (axA, axB) = plt.subplots(
        1, 2, figsize=(11.4, 3.15), gridspec_kw={"width_ratios": [1.5, 1]})
    x = np.arange(3)

    # ---- A: the frame, shrinking
    w = 0.34
    axA.bar(x - w / 2 - 0.015, f64v, width=w, color=ACCENT, zorder=3, linewidth=0)
    axA.bar(x + w / 2 + 0.015, f32v, width=w, color=AMBER, zorder=3, linewidth=0)
    for i, (a, b) in enumerate(zip(f64v, f32v)):
        axA.text(i - w / 2 - 0.015, a + 1.6, f"{a:.1f}", ha="center",
                 color=TEXT2, fontsize=10.0)
        axA.text(i + w / 2 + 0.015, b + 1.6, f"{b:.1f}", ha="center",
                 color=TEXT2, fontsize=10.0)
    axA.annotate("opt6 + f32  =  opt7\nthe shipped build",
                 xy=(2.19, 19.0), xytext=(2.36, 56),
                 arrowprops=dict(arrowstyle="->", color=AMBER, lw=1.2),
                 color=AMBER, fontsize=10.0, fontweight="bold", linespacing=1.5,
                 ha="center")
    axA.set_xlim(-0.62, 3.02)
    axA.set_ylim(0, 94)
    axA.set_yticks([0, 25, 50, 75])
    axA.set_yticklabels(["0", "25", "50", "75"], fontsize=10.0)
    axA.set_ylabel("end-to-end µs / frame", color=MUTED, fontsize=10.0)
    axA.set_xticks(x)
    axA.set_xticklabels([f"{s}\n{t}" for s, t in zip(steps, sub)],
                        fontsize=10.0, linespacing=1.6, color=TEXT2)
    axA.tick_params(axis="x", length=0, pad=7)
    bare(axA, keep=("left", "bottom"))
    axA.spines["bottom"].set_color(RULE)
    handles = [Rectangle((0, 0), 1, 1, color=c) for c in (ACCENT, AMBER)]
    axA.legend(handles, ["f64 pedestal", "f32 pedestal"], frameon=False,
               fontsize=10.0, labelcolor=TEXT2, ncol=2, loc="upper right",
               bbox_to_anchor=(1.02, 1.10), handlelength=1.1)
    axA.set_title("the frame shrinks by 2.7×", color=MUTED, fontsize=10.6,
                  pad=14, loc="left")

    # ---- B: the saving, which does not
    axB.bar(x, dv, width=0.52, color=AMBER, zorder=3, linewidth=0)
    axB.axhline(np.mean(dv), color=PALE, lw=0.9, ls="--", zorder=4)
    for i, (d, p_) in enumerate(zip(dv, pct)):
        axB.text(i, d + 0.24, f"−{d:.2f} µs", ha="center", color=PALE,
                 fontsize=12.4, fontweight="bold")
    axB.set_ylim(0, 7.6)
    axB.set_yticks([0, 2, 4])
    axB.set_yticklabels(["0", "2", "4"], fontsize=10.0)
    axB.set_ylabel("µs saved by the f32 pedestal", color=MUTED, fontsize=10.0)
    axB.set_xticks(x)
    axB.set_xticklabels([f"{s}{' †' if d else ''}\n{p:+.1f} %"
                         for s, d, p in zip(steps, dagger, pct)],
                        fontsize=10.6, linespacing=1.7, color=TEXT2)
    axB.tick_params(axis="x", length=0, pad=7)
    bare(axB, keep=("left", "bottom"))
    axB.spines["bottom"].set_color(RULE)
    axB.set_title(f"the saving does not  ·  dashed = {np.mean(dv):.2f} µs mean",
                  color=MUTED, fontsize=10.6, pad=14, loc="left")

    fig.subplots_adjust(bottom=0.30, top=0.84, left=0.055, right=0.985,
                        wspace=0.26)
    save(fig, "fig_f32_absolute")


fig_f32_absolute()


# ------------------------------------------- 10c. what the rewrite changes
def fig_variance_rewrite():
    """The rewrite in one axis: the SIZE of the numbers you subtract.

    Numbers from docs/pedestal_precision_f32_cancellation.md §4-5 and §11.
    Before, var = E[X²] − mean² subtracts two ~2.17e7 operands to recover ~2025:
    each operand sits on an f32 grid of 2 ADU², so the answer inherits ±3 — an
    ABSOLUTE error that does not shrink for quiet pixels, which is what kills
    them. After, with Y = X − X0 accumulated instead, the operands are 2025 and
    ~0.25; the grid under them is 1.2e-4 and the cancellation is simply gone.
    """
    ANS = 2025.0
    rows = [(1.0, 2.17e7, "2.17 × 10⁷", "±3 ADU²  — fatal below rms ≈ 2", MUTED),
            (0.0, ANS, "2.02 × 10³", "±0.0001 ADU²  — 30 000× smaller", AMBER)]

    fig, ax = plt.subplots(figsize=(7.7, 1.36))
    ax.set_xscale("log")
    for y, operand, mag, note, col in rows:
        if operand > ANS:
            ax.plot([ANS, operand], [y, y], color=col, lw=11, alpha=0.5,
                    solid_capstyle="butt", zorder=3)
        ax.plot([operand], [y], "o", color=col, ms=9, zorder=5)
        ax.text(operand * 2.2, y + 0.17, f"operands  {mag}", color=TEXT2,
                fontsize=9.4, va="center")
        ax.text(operand * 2.2, y - 0.19, note, color=col, fontsize=9.4,
                va="center", fontweight="bold")
    ax.axvline(ANS, color=PALE, lw=1.2, ls="--", zorder=4)
    ax.text(ANS * 1.25, -0.44, "the answer:  variance ≈ 2025", color=PALE,
            fontsize=8.8, ha="left", va="center")
    ax.annotate("", xy=(2.17e7, 1.44), xytext=(ANS, 1.44),
                arrowprops=dict(arrowstyle="<->", color=MUTED, lw=1.0))
    ax.text(2.1e5, 1.52, "4 decades of common term to cancel", color=MUTED,
            fontsize=8.8, ha="center")
    ax.set_yticks([1, 0])
    ax.set_yticklabels(["before\naccumulate X", "after\naccumulate Y = X − X₀"],
                       fontsize=10.0, linespacing=1.5)
    for lab, c in zip(ax.get_yticklabels(), (TEXT2, PALE)):
        lab.set_color(c)
    ax.tick_params(axis="y", length=0)
    ax.set_xlim(3e2, 4e9)
    ax.set_ylim(-0.62, 1.78)
    ax.set_xticks([1e3, 1e5, 1e7, 1e9])
    ax.tick_params(axis="x", labelsize=7.5)
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    fig.subplots_adjust(left=0.19, right=0.99, top=0.97, bottom=0.20)
    save(fig, "fig_variance_rewrite")


# fig_variance_rewrite()   # unplaced since slide 21 moved to A5; see above


# ---------------------------- 16. the measurement convention: s1, s4 and the floor
def fig_measure():
    """What "busy per frame" means, and why it is not the sum of durations.

    LEFT is a real schedule at 9x9 proportions, one lane per STREAM, produced by
    _schedule() rather than drawn: the copy lanes are single FIFO resources --
    H2D_overlap and D2H_overlap are 1.000 in every row of probes.csv, because the
    GPU has one copy engine per direction -- so those bars stagger, while kernels
    from different streams sit on top of each other in time. Under the lanes, the
    two union strips are computed from that same schedule, which is the whole
    point: the engine's busy time is the union of its intervals.

    No microseconds on the left panel. The schedule reproduces the SHAPE of the
    9x9 pipeline but not its exact overlap factor, and putting numbers on a
    schematic next to a panel of measured ones invites them to be read across.

    RIGHT is measured: s4 engine occupancy at 9x9 f64 cap 1700 from probes.csv
    (20.77 / 32.66 / 25.25) and the best unprofiled sustained rate from
    ladder_9x9.csv opt6 (30.01).
    """
    H, K, D = 21, 43, 25
    NF, NS = 8, 4
    frames, _ = _schedule(NF, NS, H=H, K=K, D=D)

    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(11.6, 2.95),
                                  gridspec_kw={"width_ratios": [1.62, 1]})

    # ---- left: four stream lanes, then the union each engine actually sees
    LANE, TOP = 0.52, 5.0
    for st, h0, k0, d0 in frames:
        y = TOP - st * 0.72
        for t0, dur, col in ((h0, H, AMBER), (k0, K, ACCENT), (d0, D, PALE)):
            ax.broken_barh([(t0, dur)], (y, LANE), facecolors=col,
                           edgecolor=BG, linewidth=0.8, zorder=3)
    for st in range(NS):
        ax.text(-6, TOP - st * 0.72 + LANE / 2, f"stream {st}", color=MUTED,
                fontsize=11.2, ha="right", va="center")

    def union(iv):
        pts = sorted(iv)
        out = [list(pts[0])]
        for a, b in pts[1:]:
            if a <= out[-1][1]:
                out[-1][1] = max(out[-1][1], b)
            else:
                out.append([a, b])
        return out

    lanes = [("H2D  engine busy", [(h0, h0 + H) for _, h0, _, _ in frames], AMBER),
             ("kernels  engine busy", [(k0, k0 + K) for _, _, k0, _ in frames], ACCENT)]
    ys = 1.62
    for name, iv, col in lanes:
        for a, b in union(iv):
            ax.broken_barh([(a, b - a)], (ys, 0.34), facecolors=col, zorder=3)
        ax.text(-6, ys + 0.17, name.split("  ")[0], color=col, fontsize=11.2,
                ha="right", va="center", fontweight="bold")
        ys -= 0.62

    ax.text(frames[-1][3] + D + 8, TOP - 0.72, "one copy engine\nper direction,\n"
            "so the H2D bars\nqueue", color=MUTED, fontsize=11.2, va="center",
            linespacing=1.4)
    ax.text(frames[-1][2] + K + 8, 1.45,
            "UNION — what s4 reports.\nThe kernels overlap, so it is\n"
            "shorter than their sum.", color=TEXT2, fontsize=11.2, va="center",
            linespacing=1.5)
    ax.plot([-2, frames[-1][3] + D + 2], [2.42, 2.42], color=RULE, lw=0.9, zorder=1)

    ax.set_xlim(-74, frames[-1][3] + D + 86)
    ax.set_ylim(0.75, 6.05)
    ax.set_xticks([]); ax.set_yticks([])
    bare(ax, keep=())
    ax.set_title("s4  ·  the shipped pipeline, four streams  ·  schematic, 9×9 shape",
                 color=MUTED, fontsize=11.2, loc="left", pad=10)

    # ---- right: the measured occupancies, and the two estimates of the floor
    vals = [("H2D", 20.77, AMBER), ("kernel", 32.66, ACCENT), ("D2H", 25.25, PALE)]
    xs = np.arange(3)
    ax2.bar(xs, [v for _, v, _ in vals], width=0.56,
            color=[c for _, _, c in vals], zorder=3)
    for x, (_, v, _) in zip(xs, vals):
        ax2.text(x, v + 1.0, f"{v:.2f}", ha="center", color=TEXT2, fontsize=11.8)
    ax2.axhline(32.66, color=MUTED, lw=1.0, ls="--", zorder=4)
    ax2.text(2.42, 33.3, "engine max\nprofiled  32.66", color=MUTED, fontsize=11.2,
             ha="left", va="bottom", linespacing=1.4)
    ax2.axhline(30.01, color=GREEN, lw=1.8, zorder=5)
    ax2.text(2.42, 24.4, "best sustained\nFLOOR  30.01 µs/frame\n= 33 323 FPS",
             color=GREEN, fontsize=11.2, ha="left", va="center", linespacing=1.4)
    ax2.set_xticks(xs)
    ax2.set_xticklabels([n for n, _, _ in vals], color=TEXT2, fontsize=11.8)
    ax2.set_xlim(-0.55, 5.00)
    ax2.set_ylim(0, 40)
    ax2.set_yticks([])
    bare(ax2, keep=("bottom",))
    ax2.set_title("busy µs / frame  ·  9×9 f64  ·  measured",
                  color=PALE, fontsize=11.2, loc="left", pad=6)
    fig.subplots_adjust(left=0.085, right=0.99, top=0.86, bottom=0.09, wspace=0.20)
    save(fig, "fig_measure")


fig_measure()




legibility_report()
