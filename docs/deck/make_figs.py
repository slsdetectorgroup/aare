"""Figures for docs/cf_cuda_fused.pptx — deck palette, dark.

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

Peak throughput = 1 / max(H2D, kernel, D2H), each term being that engine's BUSY
TIME PER FRAME (the union of its intervals) at the ladder's 4 streams -- and taken
as the LOWER of two estimates: the profiled engine occupancy, and the best rate the
unprofiled pipeline sustained. A sustained rate is an existence proof; the probe is
an estimate made in a loop nsys slows to ~69 us/frame, where kernels overlap less
and the union per frame reads high.

                 probe (nsys)      best sustained      PEAK              binds
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
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, Rectangle
from pathlib import Path

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

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 9,
    "text.color": PALE, "axes.labelcolor": TEXT2,
    "xtick.color": TEXT2, "ytick.color": TEXT2,
    "axes.edgecolor": RULE, "axes.facecolor": "none",
    "figure.facecolor": BG, "savefig.facecolor": BG,
    "axes.grid": False, "svg.fonttype": "none",
})


def save(fig, name):
    fig.savefig(OUT / f"{name}.png", dpi=220, transparent=False,
                bbox_inches="tight", pad_inches=0.08)
    plt.close(fig)
    print("wrote", name)


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
    ax.legend(frameon=False, fontsize=8, labelcolor=TEXT2, loc="upper right")
    ax.set_ylabel("clusters / bin", fontsize=8)
    bare(ax, keep=("left", "bottom"))
    ax.set_title("cluster energy spectrum  ·  3×3, 10 000 frames, 23.2 M clusters",
                 color=MUTED, fontsize=8.5, loc="left", pad=6)

    m = h["cpu"] > 0
    axr.axhspan(0.999, 1.001, color=GREEN, alpha=0.18, zorder=1)
    axr.axhline(1.0, color=MUTED, lw=0.8, zorder=2)
    for name, col in [("frozen", PALE), ("cuda", AMBER)]:
        axr.plot(ctr[m], h[name][m] / h["cpu"][m], color=col, lw=1.1, zorder=3)
    dev = max(np.abs(h[n][m] / h["cpu"][m] - 1).max() for n in ("frozen", "cuda"))
    axr.set_ylim(0.9955, 1.0045)
    axr.set_yticks([0.996, 1.0, 1.004])
    axr.set_yticklabels(["−0.4 %", "0", "+0.4 %"], fontsize=7.5)
    axr.set_xlabel("cluster sum [ADU]", fontsize=8)
    axr.set_ylabel("vs CPU", fontsize=8)
    bare(axr, keep=("left", "bottom"))
    axr.text(0.985, 0.90, f"worst populated bin: {dev*100:.3f} %  ·  band = ±0.1 %",
             transform=axr.transAxes, ha="right", va="top", color=GREEN,
             fontsize=7.5)
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
            color=GREEN, fontsize=8.5, ha="center", va="bottom")

    for xi, (a, b, f) in enumerate(zip(warm, cold, faults)):
        ax.text(xi - w / 2, a + 900, f"{a:,}", ha="center", va="bottom",
                color=TEXT2, fontsize=9)
        ax.text(xi + w / 2, b + 900, f"{b:,}", ha="center", va="bottom",
                color=PALE, fontsize=9.5, fontweight="bold")
        drop = 100 * (1 - b / a)
        big = drop > 10
        ax.text(xi, -2600, f"{f:,} fault" + ("" if f == 1 else "s"),
                ha="center", va="top", color=AMBER if big else MUTED,
                fontsize=8, fontweight="bold" if big else "normal")
        ax.text(xi, -6300, ("−%.0f %%" % drop) if drop >= 1 else "—",
                ha="center", va="top", color=AMBER if big else MUTED,
                fontsize=10 if big else 8.5, fontweight="bold")

    # the two steps that pay nothing, annotated just above their own bars
    for xi, yi, why in [(0, 21500, "discards every frame\nas it goes"),
                        (5, 65200, "allocates nothing\nat all")]:
        ax.text(xi, yi, why, ha="center", va="bottom", color=GREEN,
                fontsize=8, linespacing=1.35)

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=8.5, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    ax.tick_params(axis="x", pad=34)
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)

    handles = [Rectangle((0, 0), 1, 1, color=ACCENT),
               Rectangle((0, 0), 1, 1, color=AMBER)]
    ax.legend(handles, ["achievable · warm, 5-rep campaign",
                        "first run · results retained, one process"],
              frameon=False, fontsize=9, labelcolor=TEXT2, loc="upper left",
              bbox_to_anchor=(0.0, 1.055), ncol=2, handlelength=1.1)
    ax.set_title("frames / second   ·   3×3, 100 000 frames, f32   ·   "
                 "minor faults and the throughput they cost, per step",
                 color=MUTED, fontsize=9, loc="left", pad=8)
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
    for x0, x1, label, col in [(-0.5, 4.5, "ACT I · feed the GPU", ACCENT),
                               (4.5, 6.5, "ACT II · get results back", PALE),
                               (6.5, 7.5, "ACT III · kernel", AMBER)]:
        ax.axvspan(x0, x1, color=col, alpha=0.05, zorder=0)
        ax.plot([x0 + 0.08, x1 - 0.08], [TOP * 0.955] * 2, color=col, lw=2.2,
                zorder=2)
        ax.text((x0 + x1) / 2, TOP * 0.965, label, ha="center", va="bottom",
                color=col, fontsize=8.5, fontweight="bold")

    ax.bar(x, fps, width=0.62, color=colors, zorder=3, linewidth=0)

    # The H2D floor: 61 859 FPS on f64 (the nsys estimate, which this arm never
    # reached -- opt6 stops 5.4 % short) and 61 312 on f32 (the best rate actually
    # sustained). One band at this scale.
    ax.axhspan(61312, 61859, color=GREEN, alpha=0.20, zorder=1)
    ax.axhline(61859, color=GREEN, lw=1.2, ls="--", zorder=4)
    ax.text(-0.42, 63200, "H2D floor · 61–62 k FPS · the GPU cannot be fed faster",
            color=GREEN, fontsize=8.5, ha="left", va="bottom")

    for xi, (f, s) in enumerate(zip(fps, spd)):
        ax.text(xi, f + 1100, f"{f:,}", ha="center", va="bottom",
                color=PALE, fontsize=10.5, fontweight="bold")
        ax.text(xi, f - 1600, ("base" if s == 1.0 else f"×{s:.2f}"),
                ha="center", va="top", color=BG, fontsize=9, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=8.5, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("frames / second   ·   3×3 clusters, 100 000 frames, warm run   ·   "
                 "every bar against the best CPU configuration (24 threads)",
                 color=MUTED, fontsize=9, loc="left", pad=8)
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
    TOP = 56000

    for x0, x1, label, col in [(-0.5, 2.5, "ACT I", ACCENT),
                               (2.5, 4.5, "ACT II", PALE),
                               (4.5, 5.5, "ACT III", AMBER)]:
        ax.axvspan(x0, x1, color=col, alpha=0.05, zorder=0)
        ax.plot([x0 + 0.08, x1 - 0.08], [TOP * 0.955] * 2, color=col, lw=2.2,
                zorder=2)
        ax.text((x0 + x1) / 2, TOP * 0.965, label, ha="center", va="bottom",
                color=col, fontsize=8.5, fontweight="bold")

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
            color=GREEN, fontsize=8.5, va="bottom")
    ax.hlines(39775, 4.5, 5.5, color=AMBER, lw=1.3, ls="--", zorder=4)
    ax.text(5.46, 45600, "f32 D2H floor · 39 775 FPS\n(nsys estimates 39 614)",
            color=AMBER, fontsize=8.5, ha="right", va="bottom", linespacing=1.35)
    ax.add_patch(FancyArrowPatch((4.62, 34100), (4.62, 39100), arrowstyle="-|>",
                                 mutation_scale=11, color=AMBER, lw=1.5, zorder=5))
    ax.text(3.30, 37600, "−40 % kernel → D2H binds instead", color=AMBER,
            fontsize=9, ha="center", va="center", fontweight="bold")

    for xi, (f, s) in enumerate(zip(fps, spd)):
        ax.text(xi, f + 800, f"{f:,}", ha="center", va="bottom",
                color=PALE, fontsize=10.5, fontweight="bold")
        ax.text(xi, f - 1100, ("base" if s == 1.0 else f"×{s:.2f}"),
                ha="center", va="top", color=BG, fontsize=9, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=8.5, color=TEXT2)
    ax.set_ylim(0, TOP)
    ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("frames / second   ·   9×9, 20 000 frames, cap 1700 (lossless)   ·   "
                 "best CPU configuration (32 threads)   ·   opt1/opt2 are 3×3 only",
                 color=MUTED, fontsize=9, loc="left", pad=8)
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
                    fontsize=9.5, fontweight="bold")
            if e > ymax * 0.05:
                ax.text(xi, f + e / 2, f"+{e:.0f}", ha="center", va="center",
                        color=BG, fontsize=8.5, fontweight="bold")
        ax.set_xticks(x)
        ax.set_xticklabels(steps, color=TEXT2, fontsize=9)
        ax.set_ylim(0, ymax)
        ax.set_yticks([])
        bare(ax, keep=("bottom",))
        ax.set_title(title, color=PALE, fontsize=10, pad=8, loc="left")

    axes[0].set_ylabel("µs / frame", color=TEXT2)
    # label the two segments in place — the floor takes the act colour, so a
    # colour-keyed legend would be wrong
    axes[0].text(0, 16.17 / 2, "GPU\nfloor", ha="center", va="center", color=BG,
                 fontsize=8, fontweight="bold")
    axes[0].annotate("host excess", xy=(0.30, 25), xytext=(1.15, 41),
                     color=TEXT2, fontsize=8.5,
                     arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.9))

    axes[1].text(1.5, 108, "Act II removes the host bar", color=PALE, fontsize=9,
                 ha="center", fontweight="bold")
    axes[1].add_patch(FancyArrowPatch((1.5, 104), (3.25, 34), arrowstyle="-|>",
                                      mutation_scale=10, color=PALE, lw=1.3,
                                      connectionstyle="arc3,rad=-0.22"))
    axes[1].text(4.0, 52, "Act III lowers\nthe floor", color=AMBER, fontsize=9,
                 ha="center", fontweight="bold")
    axes[1].add_patch(FancyArrowPatch((4.0, 46), (4.0, 27), arrowstyle="-|>",
                                      mutation_scale=10, color=AMBER, lw=1.3))
    save(fig, "fig_overhead")


# ------------------------------------------------------ 4. streams timeline
def fig_streams():
    fig, axes = plt.subplots(3, 1, figsize=(7.7, 3.9))
    H, K, D = 12, 22, 12
    FR = H + K + D
    LANE = 0.68

    def frame(ax, lane_y, t0):
        ax.broken_barh([(t0, H)], (lane_y, LANE), facecolors=AMBER, zorder=3)
        ax.broken_barh([(t0 + H, K)], (lane_y, LANE), facecolors=ACCENT, zorder=3)
        ax.broken_barh([(t0 + H + K, D)], (lane_y, LANE), facecolors=PALE, zorder=3)

    # --- opt1: one stream, strictly serial
    ax = axes[0]
    for i in range(3):
        frame(ax, 1.0, i * FR)
    ax.set_ylim(0.4, 2.3)
    ax.text(3 * FR + 6, 1.34, "GPU idle between every stage", color=MUTED, fontsize=7.5,
            va="center")

    # --- opt2: 4 streams, barrier after each round
    ax = axes[1]
    ROUND = FR + 3 * 8
    for r in range(2):
        for st in range(4):
            frame(ax, 3 - st * 1.0, r * (ROUND + 26) + st * 8)
    ax.axvspan(ROUND, ROUND + 26, color=AMBER, alpha=0.13, zorder=1)
    ax.text(ROUND + 13, 4.15, "barrier — GPU drains", color=AMBER, fontsize=7.5,
            ha="center", va="bottom")
    ax.set_ylim(-0.4, 4.9)

    # --- opt3: no barriers, continuous
    ax = axes[2]
    for i in range(11):
        frame(ax, 3 - (i % 4) * 1.0, i * 11)
    ax.set_ylim(-1.5, 4.5)
    ax.text(0, -0.25, "streams never wait on each other — the GPU is continuously busy",
            color=ACCENT, fontsize=7.5, va="top")

    titles = ["opt1  ·  1 stream, synchronous",
              "opt2  ·  4 streams, sync barrier per round",
              "opt3  ·  4 streams, barriers removed"]
    for ax, t in zip(axes, titles):
        ax.set_xlim(-2, 190)
        ax.set_yticks([]); ax.set_xticks([])
        bare(ax, keep=())
        ax.set_title(t, color=TEXT2, fontsize=9, loc="left", pad=4)

    handles = [Rectangle((0, 0), 1, 1, color=c) for c in (AMBER, ACCENT, PALE)]
    axes[0].legend(handles, ["H2D copy", "kernel", "D2H copy"], frameon=False,
                   fontsize=8, labelcolor=TEXT2, ncol=3, loc="lower right",
                   bbox_to_anchor=(1.02, 0.98), handlelength=1.1)
    axes[2].set_xlabel("time  →", color=MUTED, fontsize=8.5, loc="left")
    fig.subplots_adjust(hspace=0.75)
    save(fig, "fig_streams")


# ------------------------------------------------------------- 5. pinning
def fig_pinning():
    fig = plt.figure(figsize=(7.7, 3.0))
    ax = fig.add_axes([0, 0.05, 0.60, 0.95]); ax.axis("off")
    ax.set_xlim(0, 10.4); ax.set_ylim(0, 6.4)

    def box(x, y, w, h, label, sub=""):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=PANEL, edgecolor=RULE, lw=1))
        ax.text(x + w / 2, y + h / 2 + 0.26, label, ha="center", va="center",
                color=PALE, fontsize=8.5, fontweight="bold")
        ax.text(x + w / 2, y + h / 2 - 0.34, sub, ha="center", va="center",
                color=MUTED, fontsize=7)

    def arrow(x0, x1, y, color, label):
        ax.add_patch(FancyArrowPatch((x0, y), (x1, y), arrowstyle="-|>",
                                     mutation_scale=10, color=color, lw=1.6))
        ax.text((x0 + x1) / 2, y + 0.22, label, ha="center", va="bottom",
                color=color, fontsize=7)

    ax.text(0, 5.95, "PAGEABLE  ·  before opt4", color=AMBER, fontsize=8.5,
            fontweight="bold")
    box(0, 4.05, 2.5, 1.1, "numpy array", "pageable")
    box(4.0, 4.05, 2.4, 1.1, "driver staging", "hidden pinned buf")
    box(7.9, 4.05, 2.5, 1.1, "GPU", "device memory")
    arrow(2.5, 4.0, 4.60, AMBER, "memcpy")
    arrow(6.4, 7.9, 4.60, AMBER, "DMA")
    ax.text(0, 3.62, "every transfer is copied twice", color=MUTED, fontsize=7)

    ax.text(0, 2.75, "PINNED  ·  opt4", color=ACCENT, fontsize=8.5, fontweight="bold")
    box(0, 0.85, 2.5, 1.1, "numpy array", "page-locked")
    box(7.9, 0.85, 2.5, 1.1, "GPU", "device memory")
    arrow(2.5, 7.9, 1.40, ACCENT, "DMA — engine reads host RAM directly")
    ax.text(0, 0.42, "no staging copy, no page faults, fully async",
            color=MUTED, fontsize=7)

    # the rule, in one inset: pinning pays only where H2D is the tallest bar
    ax2 = fig.add_axes([0.70, 0.16, 0.30, 0.62])
    x = np.arange(2)
    before = [34.26, 82.17]
    after = [25.98, 80.44]
    ax2.bar(x - 0.19, before, width=0.36, color=AMBER, zorder=3, label="opt3")
    ax2.bar(x + 0.19, after, width=0.36, color=ACCENT, zorder=3, label="opt4")
    for xi, (b, a) in enumerate(zip(before, after)):
        ax2.text(xi - 0.19, b + 2, f"{b:.0f}", ha="center", color=TEXT2, fontsize=7.5)
        ax2.text(xi + 0.19, a + 2, f"{a:.0f}", ha="center", color=TEXT2, fontsize=7.5)
        ax2.text(xi, 92, f"×{b / a:.2f}", ha="center", color=PALE, fontsize=9,
                 fontweight="bold")
    ax2.set_xticks(x)
    ax2.set_xticklabels(["3×3\nH2D-bound", "9×9\nkernel-bound"], color=TEXT2,
                        fontsize=7.5)
    ax2.set_ylim(0, 104); ax2.set_yticks([]); bare(ax2, keep=("bottom",))
    ax2.set_title("µs / frame", color=MUTED, fontsize=7.5, pad=6)
    ax2.legend(frameon=False, fontsize=7, labelcolor=TEXT2, loc="center left")
    save(fig, "fig_pinning")


# ----------------------------------------------- 6. graphs (rejected route)
def fig_graphs():
    fig = plt.figure(figsize=(7.7, 3.0))
    ax = fig.add_axes([0, 0.30, 0.66, 0.70])
    ax.axis("off"); ax.set_xlim(0, 11.2); ax.set_ylim(0, 4.6)

    def node(x, y, w, h, t, fc):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor="none"))
        ax.text(x + w / 2, y + h / 2, t, ha="center", va="center",
                color=BG, fontsize=7.5, fontweight="bold")

    ops = [("H2D", AMBER), ("kernel", ACCENT), ("D2H", PALE)] * 2

    ax.text(0, 4.15, "WITHOUT GRAPHS  ·  one driver call per operation, every frame",
            color=AMBER, fontsize=8.5, fontweight="bold")
    for i, (t, c) in enumerate(ops):
        x = 0.1 + i * 1.62
        node(x, 2.85, 1.4, 0.6, t, c)
        ax.add_patch(FancyArrowPatch((x + 0.7, 3.72), (x + 0.7, 3.52),
                                     arrowstyle="-|>", mutation_scale=7,
                                     color=MUTED, lw=0.9))
    ax.text(11.1, 3.15, "CPU cost\n≈ 6 launches", ha="right", va="center",
            color=MUTED, fontsize=7.5)

    ax.text(0, 2.18, "WITH GRAPHS  ·  record once, replay with one launch",
            color=ACCENT, fontsize=8.5, fontweight="bold")
    ax.add_patch(Rectangle((0.1, 0.72), 9.20, 1.15, facecolor=PANEL,
                           edgecolor=ACCENT, lw=1.2))
    for i, (t, c) in enumerate(ops):
        node(0.32 + i * 1.48, 0.98, 1.26, 0.6, t, c)
    ax.add_patch(FancyArrowPatch((0.8, 2.02), (0.8, 1.90), arrowstyle="-|>",
                                 mutation_scale=8, color=ACCENT, lw=1.3))
    ax.text(11.1, 1.30, "CPU cost\n≈ 1 launch", ha="right", va="center",
            color=ACCENT, fontsize=7.5, fontweight="bold")

    # the verdict
    ax2 = fig.add_axes([0.72, 0.30, 0.28, 0.62])
    x = np.arange(2)
    opt4 = [25.98, 80.44]
    graph = [25.16, 94.78]
    ax2.bar(x - 0.19, opt4, width=0.36, color=ACCENT, zorder=3, label="opt4")
    ax2.bar(x + 0.19, graph, width=0.36, color=AMBER, zorder=3, label="graphs")
    for xi, (a, g) in enumerate(zip(opt4, graph)):
        ax2.text(xi - 0.19, a + 2.5, f"{a:.0f}", ha="center", color=TEXT2, fontsize=7.5)
        ax2.text(xi + 0.19, g + 2.5, f"{g:.0f}", ha="center", color=TEXT2, fontsize=7.5)
    ax2.text(1, 108, "18 % SLOWER", ha="center", color=AMBER, fontsize=8,
             fontweight="bold")
    ax2.set_xticks(x)
    ax2.set_xticklabels(["3×3", "9×9"], color=TEXT2, fontsize=8)
    ax2.set_ylim(0, 120); ax2.set_yticks([]); bare(ax2, keep=("bottom",))
    ax2.set_title("µs / frame", color=MUTED, fontsize=7.5, pad=6)
    ax2.legend(frameon=False, fontsize=7, labelcolor=TEXT2, loc="upper left")

    fig.text(0.02, 0.10,
             "REJECTED — launch overhead stops binding one step later. The graph finder "
             "never got the chunked pipeline of opt5,\nand its original advantage is "
             "swamped by an overlap it does not have.",
             color=AMBER, fontsize=8, fontweight="bold", va="top")
    save(fig, "fig_graphs")


# -------------------------------------- 7. the result path (Act II, opt5/opt6)
def fig_resultpath():
    """Why zero-copy is worth x1.16 at 3x3 and x2.21 at 9x9: whether the host
    copy fits underneath the GPU floor."""
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11.4, 3.2))

    for ax, title, floor, copy_us, gain, verdict, col in [
        (a1, "3×3  ·  93 kB / frame", 16.17, 8.0, "×1.16",
         "copy hides under the GPU\n→ small win", ACCENT),
        (a2, "9×9  ·  467 kB / frame", 30.01, 40.0, "×2.21",
         "copy is larger than the GPU\n→ cannot hide at any overlap", AMBER),
    ]:
        ax.bar([0], [floor], width=0.5, color=ACCENT, zorder=3, linewidth=0)
        ax.bar([1], [copy_us], width=0.5, color=col, zorder=3, linewidth=0)
        ax.axhline(floor, color=GREEN, lw=1.3, ls="--", zorder=4)
        ax.text(-0.55, floor + 1.2, "GPU floor", color=GREEN, fontsize=8.5, ha="left")
        ax.text(0, floor + 1.4, f"{floor:.1f} µs", ha="center", color=PALE,
                fontsize=10, fontweight="bold")
        ax.text(1, copy_us + 1.4, f"≈{copy_us:.0f} µs", ha="center", color=PALE,
                fontsize=10, fontweight="bold")
        ax.set_xticks([0, 1])
        ax.set_xticklabels(["GPU per frame\n(H2D ∥ kernel ∥ D2H)",
                            "host copy per frame\ncollect() memcpy + malloc"],
                           color=TEXT2, fontsize=8.5)
        ax.set_xlim(-0.6, 1.7); ax.set_ylim(0, 52); ax.set_yticks([])
        bare(ax, keep=("bottom",))
        ax.set_title(title, color=PALE, fontsize=10, pad=10, loc="left")
        ax.text(1.68, 46, gain, color=col, fontsize=16, fontweight="bold", ha="right")
        ax.text(1.68, 40, "opt5 → opt6", color=MUTED, fontsize=8, ha="right")
        ax.text(-0.55, -11, verdict, color=col, fontsize=8.5, fontweight="bold",
                va="top")
    fig.subplots_adjust(bottom=0.30)
    save(fig, "fig_resultpath")


# ------------------------------------------------ 8. f32 kernel (nsys truth)
def fig_f32_kernel():
    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(7.7, 2.6),
                                  gridspec_kw={"width_ratios": [1, 1.35]})
    v = [39.93, 23.67]
    ax.bar([0, 1], v, width=0.5, color=[AMBER, ACCENT], zorder=3)
    for i, val in enumerate(v):
        ax.text(i, val + 1.2, f"{val:.1f} µs", ha="center", color=PALE,
                fontsize=11, fontweight="bold")
    ax.annotate("", xy=(1, 25.5), xytext=(0, 41.5),
                arrowprops=dict(arrowstyle="-|>", color=MUTED, lw=1.2,
                                connectionstyle="arc3,rad=-0.25"))
    ax.text(0.5, 34, "−40.7 %", ha="center", color=PALE, fontsize=10,
            fontweight="bold")
    ax.set_xticks([0, 1]); ax.set_xticklabels(["f64 pedestal", "f32 pedestal"],
                                              color=TEXT2, fontsize=8.5)
    ax.set_ylim(0, 50); ax.set_yticks([]); bare(ax, keep=("bottom",))
    ax.set_title("kernel, exclusive (nsys, 9×9, 1 stream)", color=MUTED,
                 fontsize=8, pad=8)

    labels = ["kernel", "D2H", "H2D"]
    f64 = [39.93, 19.44, 13.24]
    f32 = [23.67, 19.44, 13.24]
    y = np.arange(3); h = 0.35
    ax2.barh(y + h / 2, f64, height=h, color=AMBER, zorder=3, label="f64 ped")
    ax2.barh(y - h / 2, f32, height=h, color=ACCENT, zorder=3, label="f32 ped")
    for yi, (a, b) in enumerate(zip(f64, f32)):
        ax2.text(a + 1, yi + h / 2, f"{a:.1f}", va="center", color=TEXT2, fontsize=8)
        ax2.text(b + 1, yi - h / 2, f"{b:.1f}", va="center", color=TEXT2, fontsize=8)
    ax2.set_yticks(y); ax2.set_yticklabels(labels, color=TEXT2, fontsize=8.5)
    ax2.invert_yaxis(); ax2.set_xlim(0, 52); ax2.set_xticks([])
    bare(ax2, keep=("left",))
    ax2.legend(frameon=False, fontsize=8, labelcolor=TEXT2, loc="lower right")
    ax2.set_title("per-frame GPU operations (µs) — only the kernel moves",
                  color=MUTED, fontsize=8, pad=8)
    save(fig, "fig_f32_kernel")


# --------------------------------------------------------- 9. cancellation
def fig_cancellation():
    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(7.7, 2.7),
                                  gridspec_kw={"width_ratios": [1.25, 1]})
    names = ["E[X²]\n2.17e7", "mean²\n2.17e7", "variance\n2025"]
    vals = [2.17e7, 2.17e7, 2025]
    ax.bar([0, 1], vals[:2], width=0.5, color=[PALE, PALE], zorder=3)
    ax.bar([2], [2025], width=0.5, color=AMBER, zorder=3)
    ax.set_yscale("log"); ax.set_ylim(1e2, 2e8)
    ax.set_xticks([0, 1, 2]); ax.set_xticklabels(names, color=TEXT2, fontsize=8)
    ax.set_yticks([1e3, 1e5, 1e7])
    ax.axhline(2048, color=ACCENT, lw=1.3, ls="--", zorder=4)
    ax.text(2.42, 3000, "f32 rounding step\nat 2.17e7  =  2048", color=ACCENT,
            fontsize=7.5, ha="right", va="bottom")
    bare(ax)
    ax.set_title("var = E[X²] − mean²   (f32, mean ≈ 4655 ADU)",
                 color=MUTED, fontsize=8, pad=8)

    rms = np.linspace(0, 12, 200)
    ax2.fill_between(rms, 0, np.where(rms < 6.5, 1, 0), color=AMBER, alpha=0.16,
                     step="pre")
    ax2.plot(rms, rms**2, color=PALE, lw=1.8, label="true variance")
    ax2.axhline(42, color=ACCENT, lw=1.4, ls="--", label="f32 error floor")
    ax2.set_xlabel("pixel rms (ADU)", color=TEXT2, fontsize=8.5)
    ax2.set_ylabel("variance", color=TEXT2, fontsize=8.5)
    ax2.set_ylim(0, 150); ax2.set_xlim(0, 12)
    ax2.set_yticks([]); ax2.tick_params(labelsize=8)
    bare(ax2)
    ax2.text(1.0, 108, "quiet pixels:\nerror > variance\n→ rms clamped to 0\n→ fires every frame",
             color=AMBER, fontsize=7.5, va="top")
    ax2.legend(frameon=False, fontsize=7.5, labelcolor=TEXT2, loc="lower right")
    save(fig, "fig_cancellation")


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
                    fontsize=11.5, fontweight="bold")
            ax.text(xi, p_ + 2.0, "resolvable to 0.0 pts", ha="center",
                    color=AMBER, fontsize=8)
        else:
            ax.text(xi, h_ + 1.6, f"{l_:+.0f} … {h_:+.0f}%", ha="center",
                    color=TEXT2, fontsize=9.5)

    ax.set_xticks(x); ax.set_xticklabels(steps, color=TEXT2, fontsize=9)
    ax.set_xlim(-0.6, 3.6)
    ax.set_ylim(-34, 30)
    ax.set_yticks([-20, 0, 20])
    ax.set_yticklabels(["−20 %", "0", "+20 %"], fontsize=8)
    bare(ax, keep=("left", "bottom"))
    ax.spines["bottom"].set_color(RULE)
    ax.set_title("end-to-end change from the SAME −40 % kernel, 9×9   ·   "
                 "bar = measurement, band = what the reps allow",
                 color=MUTED, fontsize=9, pad=10, loc="left")
    ax.text(1.5, -29.5, "through an allocating result path the effect is not measurable "
            "— the band straddles zero", color=MUTED, fontsize=8.5,
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
                ha="center", color=PALE if d else MUTED, fontsize=8.5)
    ax.axhline(0.01, color=PALE, lw=1.2, ls="--")
    ax.text(4.4, 0.0104, "0.01% — well inside statistical noise", color=PALE,
            fontsize=8, ha="right")
    ax.set_xticks(x); ax.set_xticklabels(names, color=TEXT2, fontsize=8.5)
    ax.set_ylim(0, 0.0125); ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.set_title("cluster-count difference vs CPU  ·  233 M clusters, 3×3",
                 color=MUTED, fontsize=8.5, pad=8)
    save(fig, "fig_correctness")


for f in (fig_arc, fig_arc_9x9, fig_first_run, fig_overhead, fig_streams, fig_pinning,
          fig_graphs, fig_resultpath, fig_f32_kernel, fig_cancellation,
          fig_bottleneck, fig_correctness):
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
                color=tcol, fontsize=8.5, fontweight="bold", zorder=4)

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
                color=TEXT2, fontsize=9)

    ax.text(-0.25, yG + lane_h + 0.30, "submit → collect, serialized   (opt4)",
            ha="left", va="bottom", color=TEXT2, fontsize=9.5, fontweight="bold")
    ax.text(-0.25, yG2 + lane_h + 0.30,
            "submit(i+1) before collect(i)   (opt5)",
            ha="left", va="bottom", color=AMBER, fontsize=9.5, fontweight="bold")

    for x, y0, y1, col in ((serial_end, yH, yG + lane_h, MUTED),
                           (pipe_end, yH2, yG2 + lane_h, AMBER)):
        ax.plot([x, x], [y0 - 0.18, y1 + 0.18], color=col, lw=1.2, ls="--",
                zorder=5)

    ax.annotate("", xy=(pipe_end, 0.42), xytext=(serial_end, 0.42),
                arrowprops=dict(arrowstyle="<|-|>", color=AMBER, lw=1.5))
    ax.text((pipe_end + serial_end) / 2, 0.20,
            "saved: min(GPU, host) per chunk", ha="center", va="top",
            color=AMBER, fontsize=9.5, fontweight="bold")

    ax.text(serial_end + 0.25, yG + lane_h / 2, "GPU + host  per chunk",
            ha="left", va="center", color=MUTED, fontsize=9)
    ax.text(pipe_end + 0.25, yG2 + lane_h / 2, "max(GPU, host)  per chunk",
            ha="left", va="center", color=AMBER, fontsize=9, fontweight="bold")

    ax.set_xlim(-1.6, serial_end + 4.0)
    ax.set_ylim(0, 4.25)
    ax.axis("off")
    save(fig, "fig_overlap")


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
    rows = [("ClusterFinder\nserial CPU", 2.75, ACCENT, True),
            ("ClusterFinderFrozen\nCPU twin", 1.55, PALE, False),
            ("ClusterFinderCUDA", 0.35, AMBER, False)]

    for label, y, col, stair in rows:
        ax.text(-0.35, y + 0.26, label, ha="right", va="center", color=TEXT2,
                fontsize=9.5, linespacing=1.35)
        ax.plot([0, XE], [y, y], color=RULE, lw=1.0, zorder=1)

        if stair:
            xs = np.linspace(0, XF, 22)
            ys = y + 0.10 + 0.42 * xs / XF
            ax.step(xs, ys, where="post", color=col, lw=2.2, zorder=3)
            ax.plot([XF, XE], [y + 0.52, y + 0.62], color=col, lw=2.0, ls=":",
                    zorder=3)
            ax.text(XF / 2, y + 0.64, "the pedestal moves DURING the scan",
                    ha="center", va="bottom", color=col, fontsize=9,
                    fontweight="bold")
            ax.text(XF / 2, y - 0.13,
                    "a pixel late in the frame is judged against a pedestal that\n"
                    "already contains this frame's earlier pixels",
                    ha="center", va="top", color=MUTED, fontsize=8,
                    linespacing=1.4)
        else:
            ax.plot([0, XF], [y + 0.10] * 2, color=col, lw=2.2, zorder=3)
            ax.plot([XF, XF], [y + 0.10, y + 0.52], color=col, lw=2.2, zorder=3)
            ax.plot([XF, XE], [y + 0.52] * 2, color=col, lw=2.0, ls=":", zorder=3)
            ax.text(XF / 2, y + 0.18, "every decision uses the frame-start snapshot",
                    ha="center", va="bottom", color=col, fontsize=9,
                    fontweight="bold")

    ax.axvline(XF, color=MUTED, lw=1.1, ls="--", zorder=2, ymin=0.02, ymax=0.93)
    ax.text(XF / 2, 3.80, "one frame  ·  160 000 pixels in raster order",
            ha="center", va="bottom", color=MUTED, fontsize=9.5)
    ax.text(XF + 0.12, 3.80, "frame ends →\nupdates applied", ha="left",
            va="bottom", color=MUTED, fontsize=8.5, linespacing=1.35)

    # what the middle row buys: two comparisons, one variable each
    XA = 8.35
    ax.annotate("", xy=(XA, 1.72), xytext=(XA, 2.92),
                arrowprops=dict(arrowstyle="<|-|>", color=PALE, lw=1.5))
    ax.text(XA + 0.18, 2.32, "update TIMING\narithmetic held fixed", ha="left",
            va="center", color=PALE, fontsize=9, fontweight="bold",
            linespacing=1.4)
    ax.annotate("", xy=(XA, 0.52), xytext=(XA, 1.72),
                arrowprops=dict(arrowstyle="<|-|>", color=AMBER, lw=1.5))
    ax.text(XA + 0.18, 1.12, "the PORT\ntiming held fixed", ha="left",
            va="center", color=AMBER, fontsize=9, fontweight="bold",
            linespacing=1.4)

    ax.set_xlim(-2.9, 12.0)
    ax.set_ylim(-0.35, 4.30)
    ax.axis("off")
    save(fig, "fig_pedtiming")


fig_overlap()
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

    fig, axes = plt.subplots(1, 2, figsize=(11.2, 4.3))
    for ax, name, col in ((axes[0], "frozen", PALE), (axes[1], "cuda", ACCENT)):
        w, m = sub[name], msk[name]
        ax.imshow(np.ma.masked_where(~m, w), cmap=cmap, vmin=0, vmax=vmax,
                  interpolation="nearest", origin="upper")
        for i in range(w.shape[0]):
            for j in range(w.shape[1]):
                if m[i, j]:
                    ax.text(j, i, f"{w[i, j]:.0f}", ha="center", va="center",
                            fontsize=4.6,
                            color="white" if w[i, j] < 0.55 * vmax else "black")
        for (cx, cy) in cen[name]:
            ax.plot(cx - X0, cy - Y0, ".", color="#FF4B4B", ms=6, zorder=5)
        for (cx, cy) in only:
            if name == "cuda":
                ax.add_patch(plt.Circle((cx - X0, cy - Y0), 2.6, fill=False,
                                        ec=AMBER, lw=2.0, zorder=6))
            else:
                ax.add_patch(plt.Circle((cx - X0, cy - Y0), 2.6, fill=False,
                                        ec=AMBER, lw=1.4, ls=":", zorder=6))
        ax.set_title(f"{name}  —  {d['n_' + name]:,} clusters in this frame",
                     color=col, fontsize=10, fontweight="bold", pad=7)
        ax.set_xticks([]); ax.set_yticks([])
        for sp in ax.spines.values():
            sp.set_color(RULE)

    axes[0].text(-0.03, 0.5, f"frame {d['fid']}\nzoom on (202, 8)",
                 transform=axes[0].transAxes, rotation=90, ha="right",
                 va="center", color=MUTED, fontsize=8, linespacing=1.4)
    # anchored in DATA coordinates: the window is no longer square (the cut is
    # clipped by the top edge of the frame), so axes fractions do not track the
    # pixel once matplotlib letterboxes the image to keep aspect.
    (ox, oy), = only
    axes[1].annotate("cuda keeps this one;\nfrozen does not",
                     xy=(ox - X0 + 2.9, oy - Y0), xytext=(0.99, 0.93),
                     xycoords="data", textcoords="axes fraction",
                     color=AMBER, fontsize=8.5, fontweight="bold", ha="right",
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
    fig, ax = plt.subplots(figsize=(11.2, 2.55))

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
            ax.text(frac + 1.5, y, f"{frac:.0f} %", va="center", fontsize=9,
                    color=PALE if full else TEXT2,
                    fontweight="bold" if full else "normal")
            ticks.append(y); labels.append(f"{name}  ·  {kind}")
            y -= 1.0
        y -= 0.45

    ax.axvline(100, color=MUTED, lw=1.1, ls="--", zorder=4)
    ax.text(99, 1.05, "capacity of one SM", color=MUTED, fontsize=8.5,
            ha="right")
    ax.set_yticks(ticks)
    ax.set_yticklabels(labels, color=TEXT2, fontsize=9)
    ax.set_xlim(0, 152); ax.set_xticks([])
    ax.set_ylim(y + 0.6, 1.5)
    bare(ax, keep=("left",))
    ax.tick_params(axis="y", length=0)

    ax.text(115, ticks[0] - 0.5, "6 blocks resident\n38 × 256 = 9 728 regs each",
            color=ACCENT, fontsize=8.5, va="center", linespacing=1.4)
    ax.text(115, ticks[2] - 0.5, "2 blocks resident\n128 × 256 = 32 768 regs each",
            color=AMBER, fontsize=8.5, va="center", linespacing=1.4)
    fig.subplots_adjust(left=0.16, right=0.99, top=0.88, bottom=0.06)
    save(fig, "fig_regpressure")


fig_regpressure()
