"""Figures for ClusterFinderCUDA_optimizations.pptx — deck palette, dark, transparent."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyArrowPatch, Rectangle
from pathlib import Path

OUT = Path(__file__).parent / "figs"
OUT.mkdir(exist_ok=True)

BG     = "#0B1018"
PANEL  = "#121A28"
RULE   = "#1E2836"
ACCENT = "#1E90C2"   # data 1
AMBER  = "#E8B25C"   # data 2
PALE   = "#E7EDF4"   # data 3 / primary text
TEXT2  = "#A5B2C4"
MUTED  = "#6B7A90"   # non-data only: grid, axes, annotation

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


# ---------------------------------------------------------------- 1. the arc
def fig_arc():
    steps = ["CPU MT\n48 threads", "opt1\n1 stream", "opt2\nstreams+batch",
             "opt3\npipeline", "opt4\npinned", "opt5\ngraphs"]
    fps = [4761, 14968, 23134, 26588, 36810, 39472]
    spd = [1.0, 3.14, 4.86, 5.58, 7.73, 8.29]
    colors = [MUTED] + [ACCENT] * 4 + [AMBER]

    fig, ax = plt.subplots(figsize=(11.4, 3.5))
    x = np.arange(len(steps))
    bars = ax.bar(x, fps, width=0.62, color=colors, zorder=3)
    for b in bars:
        b.set_linewidth(0)
    for xi, (f, s) in enumerate(zip(fps, spd)):
        ax.text(xi, f + 900, f"{f:,}", ha="center", va="bottom",
                color=PALE, fontsize=11, fontweight="bold")
        ax.text(xi, f + 3100, ("baseline" if s == 1.0 else f"×{s:.2f}"),
                ha="center", va="bottom", color=AMBER if s > 8 else TEXT2, fontsize=9)
    ax.set_xticks(x)
    ax.set_xticklabels(steps, fontsize=9, color=TEXT2)
    ax.set_ylim(0, 47000)
    ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.spines["bottom"].set_color(RULE)
    ax.set_ylabel("")
    ax.text(0, 45500, "frames / second   ·   3×3 clusters, 100 000 frames, warm run",
            color=MUTED, fontsize=9, ha="left")
    save(fig, "fig_arc")


# ------------------------------------------------- 2. host overhead collapse
def fig_overhead():
    steps = ["opt1", "opt2", "opt3", "opt4"]
    ovhd = [44, 21, 14, 3]
    kern = [23, 22, 24, 24]

    fig, ax = plt.subplots(figsize=(5.6, 3.0))
    x = np.arange(len(steps))
    ax.bar(x, kern, width=0.55, color=ACCENT, zorder=3, label="kernel (GPU)")
    ax.bar(x, ovhd, width=0.55, bottom=kern, color=AMBER, zorder=3,
           label="host + PCIe overhead")
    for xi, (k, o) in enumerate(zip(kern, ovhd)):
        ax.text(xi, k + o + 1.6, f"{o} µs", ha="center", color=AMBER,
                fontsize=10, fontweight="bold")
    ax.set_xticks(x); ax.set_xticklabels(steps, color=TEXT2)
    ax.set_ylabel("µs / frame", color=TEXT2)
    ax.set_ylim(0, 78)
    bare(ax)
    ax.legend(frameon=False, fontsize=8.5, labelcolor=TEXT2, loc="upper right")
    save(fig, "fig_overhead")


# ------------------------------------------------------ 3. streams timeline
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


# ------------------------------------------------------------- 4. pinning
def fig_pinning():
    fig = plt.figure(figsize=(7.7, 3.0))
    ax = fig.add_axes([0, 0.05, 0.63, 0.95]); ax.axis("off")
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

    ax2 = fig.add_axes([0.75, 0.16, 0.25, 0.66])
    v = [14, 3]
    ax2.bar([0, 1], v, width=0.55, color=[AMBER, ACCENT], zorder=3)
    for i, val in enumerate(v):
        ax2.text(i, val + 0.5, f"{val} µs", ha="center", color=PALE,
                 fontsize=10, fontweight="bold")
    ax2.set_xticks([0, 1])
    ax2.set_xticklabels(["opt3\npageable", "opt4\npinned"], color=TEXT2, fontsize=8)
    ax2.set_ylim(0, 18); ax2.set_yticks([]); bare(ax2, keep=("bottom",))
    ax2.set_title("host overhead / frame", color=MUTED, fontsize=7.5, pad=6)
    save(fig, "fig_pinning")


# -------------------------------------------------------------- 5. graphs
def fig_graphs():
    fig, ax = plt.subplots(figsize=(7.7, 2.6))
    ax.axis("off"); ax.set_xlim(0, 12.6); ax.set_ylim(0, 4.6)

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
    ax.text(12.5, 3.15, "CPU cost\n≈ 6 launches", ha="right", va="center",
            color=MUTED, fontsize=7.5)

    ax.text(0, 2.18, "WITH GRAPHS  ·  opt5  ·  record once, replay with one launch",
            color=ACCENT, fontsize=8.5, fontweight="bold")
    ax.add_patch(Rectangle((0.1, 0.72), 9.85, 1.15, facecolor=PANEL,
                           edgecolor=ACCENT, lw=1.2))
    for i, (t, c) in enumerate(ops):
        node(0.38 + i * 1.58, 0.98, 1.34, 0.6, t, c)
    ax.add_patch(FancyArrowPatch((0.8, 2.02), (0.8, 1.90), arrowstyle="-|>",
                                 mutation_scale=8, color=ACCENT, lw=1.3))
    ax.text(12.5, 1.30, "CPU cost\n≈ 1 launch", ha="right", va="center",
            color=ACCENT, fontsize=7.5, fontweight="bold")
    ax.text(0.1, 0.32, "cudaGraphLaunch()  —  the whole DAG is submitted as one unit; "
                       "the driver already knows every dependency",
            color=MUTED, fontsize=7)
    save(fig, "fig_graphs")


# ------------------------------------------------ 6. f32 kernel (nsys truth)
def fig_f32_kernel():
    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(7.7, 2.6),
                                  gridspec_kw={"width_ratios": [1, 1.35]})
    v = [43.0, 25.6]
    ax.bar([0, 1], v, width=0.5, color=[AMBER, ACCENT], zorder=3)
    for i, val in enumerate(v):
        ax.text(i, val + 1.2, f"{val} µs", ha="center", color=PALE,
                fontsize=11, fontweight="bold")
    ax.annotate("", xy=(1, 27.5), xytext=(0, 44.5),
                arrowprops=dict(arrowstyle="-|>", color=MUTED, lw=1.2,
                                connectionstyle="arc3,rad=-0.25"))
    ax.text(0.5, 37, "−40%", ha="center", color=PALE, fontsize=10,
            fontweight="bold")
    ax.set_xticks([0, 1]); ax.set_xticklabels(["f64 pedestal", "f32 pedestal"],
                                              color=TEXT2, fontsize=8.5)
    ax.set_ylim(0, 52); ax.set_yticks([]); bare(ax, keep=("bottom",))
    ax.set_title("kernel, exclusive (nsys, 9×9)", color=MUTED, fontsize=8, pad=8)

    labels = ["kernel", "D2H", "H2D"]
    f64 = [43.0, 19.4, 13.2]
    f32 = [25.6, 19.8, 13.5]
    y = np.arange(3); h = 0.35
    ax2.barh(y + h / 2, f64, height=h, color=AMBER, zorder=3, label="f64 ped")
    ax2.barh(y - h / 2, f32, height=h, color=ACCENT, zorder=3, label="f32 ped")
    for yi, (a, b) in enumerate(zip(f64, f32)):
        ax2.text(a + 1, yi + h / 2, f"{a:.1f}", va="center", color=TEXT2, fontsize=8)
        ax2.text(b + 1, yi - h / 2, f"{b:.1f}", va="center", color=TEXT2, fontsize=8)
    ax2.set_yticks(y); ax2.set_yticklabels(labels, color=TEXT2, fontsize=8.5)
    ax2.invert_yaxis(); ax2.set_xlim(0, 56); ax2.set_xticks([])
    bare(ax2, keep=("left",))
    ax2.legend(frameon=False, fontsize=8, labelcolor=TEXT2, loc="lower right")
    ax2.set_title("per-frame GPU operations (µs)", color=MUTED, fontsize=8, pad=8)
    save(fig, "fig_f32_kernel")


# ------------------------------------------------------- 7. cancellation
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


# ------------------------------------------------- 8. where f32 pays or not
def fig_bottleneck():
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11.4, 3.0))

    for ax, title, kern, floor, gain in [
        (a1, "3×3 clusters — pipeline-bound", (24, 13), 25,
         "kernel already hidden  →  0% end-to-end"),
        (a2, "9×9 clusters — kernel-bound (f64)", (43, 26), 32,
         "kernel on the critical path  →  −8% wall"),
    ]:
        x = [0, 1]
        ax.bar(x, kern, width=0.5, color=[AMBER, ACCENT], zorder=3)
        ax.axhline(floor, color=PALE, lw=1.4, ls="--", zorder=4)
        ax.text(1.62, floor + 1.2, "transfer + host floor", color=PALE,
                fontsize=8, ha="right")
        for i, v in enumerate(kern):
            ax.text(i, v + 1.2, f"{v} µs", ha="center", color=PALE,
                    fontsize=10, fontweight="bold")
        ax.set_xticks(x); ax.set_xticklabels(["f64 pedestal", "f32 pedestal"],
                                             color=TEXT2, fontsize=9)
        ax.set_xlim(-0.6, 1.7); ax.set_ylim(0, 55); ax.set_yticks([])
        bare(ax, keep=("bottom",))
        ax.set_title(title, color=PALE, fontsize=9.5, pad=10)
        ax.text(-0.55, -9, gain, color=AMBER if "0%" in gain else ACCENT,
                fontsize=8.5, fontweight="bold")
    fig.subplots_adjust(bottom=0.22)
    save(fig, "fig_bottleneck")


# ----------------------------------------------------------- 9. correctness
def fig_correctness():
    fig, ax = plt.subplots(figsize=(7.4, 2.4))
    names = ["CPU MT", "opt1", "opt2", "opt3", "opt4", "opt5", "opt6 (f32)"]
    diff = [0.0, 0.0040, 0.0035, 0.0035, 0.0035, 0.0035, 0.0039]
    colors = [MUTED] + [ACCENT] * 5 + [AMBER]
    x = np.arange(len(names))
    ax.bar(x, diff, width=0.55, color=colors, zorder=3)
    for xi, d in enumerate(diff):
        ax.text(xi, d + 0.00022, ("reference" if d == 0 else f"{d:.4f}%"),
                ha="center", color=PALE if d else MUTED, fontsize=8.5)
    ax.axhline(0.01, color=PALE, lw=1.2, ls="--")
    ax.text(6.4, 0.0104, "0.01% — well inside statistical noise", color=PALE,
            fontsize=8, ha="right")
    ax.set_xticks(x); ax.set_xticklabels(names, color=TEXT2, fontsize=8.5)
    ax.set_ylim(0, 0.0125); ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.set_title("cluster-count difference vs CPU  ·  233 million clusters, 3×3",
                 color=MUTED, fontsize=8.5, pad=8)
    save(fig, "fig_correctness")


for f in (fig_arc, fig_overhead, fig_streams, fig_pinning, fig_graphs,
          fig_f32_kernel, fig_cancellation, fig_bottleneck, fig_correctness):
    f()
print("done ->", OUT)
