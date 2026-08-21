"""Figures for the fused deck — kernel/algorithm/occupancy half.

Same palette and conventions as make_figs.py (deck palette, dark, tight bbox).
Writes into docs/figures/ alongside the optimization figures.

Occupancy numbers are not hand-computed: they come from
cudaOccupancyMaxActiveBlocksPerMultiprocessor + cudaFuncGetAttributes on the
real kernel (RTX 4090, sm_89), measured 2026-08-11 — see the table in
build_fused_deck.py.
"""
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import LinearSegmentedColormap
from matplotlib.patches import Rectangle, FancyArrowPatch
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "figures"
OUT.mkdir(exist_ok=True)

BG     = "#0B1018"
PANEL  = "#121A28"
RULE   = "#1E2836"
ACCENT = "#1E90C2"
AMBER  = "#E8B25C"
PALE   = "#E7EDF4"
TEXT2  = "#A5B2C4"
MUTED  = "#6B7A90"

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 9,
    "text.color": PALE, "axes.labelcolor": TEXT2,
    "xtick.color": TEXT2, "ytick.color": TEXT2,
    "axes.edgecolor": RULE, "axes.facecolor": "none",
    "figure.facecolor": BG, "savefig.facecolor": BG,
    "axes.grid": False, "svg.fonttype": "none",
})

# deck-native sequential map: background → accent → amber → white-hot
CMAP = LinearSegmentedColormap.from_list(
    "deck", ["#080C12", "#10202F", ACCENT, AMBER, "#FFF3DC"])


def save(fig, name):
    fig.savefig(OUT / f"{name}.png", dpi=220, transparent=False,
                bbox_inches="tight", pad_inches=0.08)
    plt.close(fig)
    print("wrote", name)


def bare(ax, keep=("left", "bottom")):
    for s in ("top", "right", "left", "bottom"):
        ax.spines[s].set_visible(s in keep)


# ------------------------------------------------ 1. what a frame looks like
def fig_frame():
    """Pedestal-subtracted MOENCH frame + a zoom on one real 3×3 cluster."""
    sys.path.append("/home/ferjao_k/aare/build")
    from aare import File
    base = Path("/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/"
                "2026032408/process/xrf/")
    pd = File(base / "Cu_factor_10_pedestal_master_0.json")
    ped = np.mean([np.asarray(pd.read_frame(), dtype=np.float64)
                   for _ in range(200)], axis=0)
    f = File(base / "Cu_factor_10_data_master_0.json")
    frame = np.asarray(f.read_frame(), dtype=np.float64) - ped

    crop = frame[40:190, 40:190]

    fig = plt.figure(figsize=(7.4, 3.25))
    ax = fig.add_axes([0.0, 0.02, 0.44, 0.94])
    im = ax.imshow(crop, cmap=CMAP, vmin=-40, vmax=1200, interpolation="nearest")
    ax.set_xticks([]); ax.set_yticks([])
    for s in ax.spines.values():
        s.set_color(RULE)
    ax.set_title("one frame, pedestal subtracted   ·   150×150 crop",
                 color=MUTED, fontsize=8, pad=7)
    cb = fig.colorbar(im, ax=ax, fraction=0.045, pad=0.02)
    cb.outline.set_edgecolor(RULE)
    cb.ax.tick_params(labelsize=7, color=RULE)
    cb.set_label("ADU above pedestal", color=MUTED, fontsize=7.5)

    # Pick a clean, well-isolated charge-sharing event: a local maximum of
    # moderate amplitude whose 3×3 core carries the charge and whose
    # surrounding ring is quiet — i.e. what the algorithm is designed to find.
    best, win = None, None
    for r in range(6, frame.shape[0] - 6):
        for c in range(6, frame.shape[1] - 6):
            v = frame[r, c]
            if not (500 < v < 2000):
                continue
            w = frame[r - 4:r + 5, c - 4:c + 5]
            if w.max() > v:                       # must be the local max
                continue
            core = w[3:6, 3:6]
            ring = np.concatenate([w[:3].ravel(), w[6:].ravel(),
                                   w[3:6, :3].ravel(), w[3:6, 6:].ravel()])
            if ring.max() > 80:                   # neighbourhood must be quiet
                continue
            share = (core.sum() - v) / core.sum()  # charge outside the peak
            if best is None or share > best[0]:
                best, win = (share, r, c), w
    if win is None:                                # fallback: brightest pixel
        inner = frame[6:-6, 6:-6]
        r, c = np.unravel_index(np.argmax(inner), inner.shape)
        win = frame[r + 2:r + 11, c + 2:c + 11]

    ax2 = fig.add_axes([0.60, 0.10, 0.30, 0.78])
    ax2.imshow(win, cmap=CMAP, vmin=-40, vmax=1200, interpolation="nearest")
    ax2.set_xticks([]); ax2.set_yticks([])
    for s in ax2.spines.values():
        s.set_color(RULE)
    ax2.add_patch(Rectangle((2.5, 2.5), 3, 3, fill=False, edgecolor=PALE,
                            lw=1.8, zorder=5))
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            v = win[4 + dy, 4 + dx]
            ax2.text(4 + dx, 4 + dy, f"{v:.0f}", ha="center", va="center",
                     color=BG if v > 500 else PALE, fontsize=7,
                     fontweight="bold" if dx == 0 and dy == 0 else "normal",
                     zorder=6)
    ax2.set_title("9×9 zoom on one hit", color=MUTED, fontsize=8, pad=7)
    ax2.text(4, 9.2, f"3×3 sum = {win[3:6, 3:6].sum():.0f} ADU — one photon.\n"
                     "The peak pixel holds only part of the charge.",
             ha="center", va="top", color=TEXT2, fontsize=7.5)
    save(fig, "fig_frame")


# --------------------------------------------- 2. shared-memory tile + halo
def fig_tile():
    B, r = 16, 1                       # 16×16 block, 3×3 cluster → 1-px halo
    n = B + 2 * r
    fig = plt.figure(figsize=(7.6, 3.1))

    ax = fig.add_axes([0.0, 0.0, 0.44, 1.0])
    ax.set_aspect("equal"); ax.axis("off")
    ax.set_xlim(-0.6, n + 0.6); ax.set_ylim(-3.2, n + 1.3)
    for i in range(n):
        for j in range(n):
            halo = i < r or j < r or i >= n - r or j >= n - r
            ax.add_patch(Rectangle((j, n - 1 - i), 0.92, 0.92,
                                   facecolor=RULE if halo else "#17394F",
                                   edgecolor="none"))
    # one thread's 3×3 neighbourhood
    ti, tj = 6, 5
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ax.add_patch(Rectangle((tj + r + dj, n - 1 - (ti + r + di)), 0.92,
                                   0.92, facecolor=ACCENT, edgecolor="none"))
    ax.add_patch(Rectangle((tj + r, n - 1 - (ti + r)), 0.92, 0.92,
                           facecolor=AMBER, edgecolor="none"))
    ax.text(n / 2, n + 0.45, "shared-memory tile   ·   18 × 18",
            ha="center", color=MUTED, fontsize=8)
    for y, c, t in [(-1.05, AMBER, "the thread's own pixel"),
                    (-1.85, ACCENT, "its 3×3 neighbourhood"),
                    (-2.65, RULE, "halo — loaded, never centred on")]:
        ax.add_patch(Rectangle((0, y), 0.7, 0.36, facecolor=c, edgecolor="none"))
        ax.text(1.0, y + 0.18, t, va="center", color=TEXT2, fontsize=7.5)

    # right: tile cost vs cluster size
    ax2 = fig.add_axes([0.575, 0.20, 0.40, 0.62])
    labels = ["3×3\n18×18", "5×5\n20×20", "7×7\n22×22", "9×9\n24×24"]
    kb = [(16 + 2 * (k // 2)) ** 2 * 4 / 1024 for k in (3, 5, 7, 9)]
    ax2.bar(np.arange(4), kb, width=0.55, color=ACCENT, zorder=3)
    for i, v in enumerate(kb):
        ax2.text(i, v + 0.12, f"{v:.1f}", ha="center", color=PALE, fontsize=9,
                 fontweight="bold")
    ax2.axhline(100, color=PALE, lw=1.2, ls="--")
    ax2.set_xticks(np.arange(4)); ax2.set_xticklabels(labels, color=TEXT2,
                                                      fontsize=8)
    ax2.set_ylim(0, 3.4); ax2.set_yticks([])
    bare(ax2, keep=("bottom",))
    ax2.set_title("KB of shared memory per 16×16 block  (float tile)",
                  color=MUTED, fontsize=8, pad=8)
    ax2.text(3.55, 3.15, "100 KB available per SM on Ada\n"
                         "— shared memory is never the limit",
             ha="right", va="top", color=PALE, fontsize=7.5)
    save(fig, "fig_tile")


# ------------------------------------------------- 3. occupancy / registers
def fig_occupancy():
    fig, (ax, ax2) = plt.subplots(1, 2, figsize=(11.2, 2.95),
                                  gridspec_kw={"width_ratios": [1, 1.5]})

    # left — registers set the occupancy, per cluster size
    occ = [100.0, 33.3]
    ax.bar([0, 1], occ, width=0.5, color=[ACCENT, AMBER], zorder=3)
    for i, o in enumerate(occ):
        ax.text(i, o + 3, f"{o:.0f}%", ha="center", color=PALE, fontsize=12,
                fontweight="bold")
    ax.set_xticks([0, 1])
    ax.set_xticklabels(["3×3 cluster\n38 regs/thread · 6 blocks/SM",
                        "9×9 cluster\n128 regs/thread · 2 blocks/SM"],
                       color=TEXT2, fontsize=8.5)
    ax.set_ylim(0, 122); ax.set_yticks([])
    bare(ax, keep=("bottom",))
    ax.set_title("achieved occupancy, 16×16 block  ·  f32 build", color=MUTED,
                 fontsize=8.5, pad=8)

    # right — block-size sweep, both cluster sizes
    o3 = [100.0, 100.0, 66.7]
    o9 = [33.3, 33.3, 0.0]
    halo3 = [56, 27, 13]
    blocks = [f"{b}\nhalo +{h}% of the tile"
              for b, h in zip(["8×8 · 64 threads", "16×16 · 256 threads",
                               "32×32 · 1024 threads"], halo3)]
    x = np.arange(3); w = 0.34
    ax2.bar(x - w / 2, o3, width=w, color=ACCENT, zorder=3, label="3×3 cluster")
    ax2.bar(x + w / 2, o9, width=w, color=AMBER, zorder=3, label="9×9 cluster")
    for xi, (a, b) in enumerate(zip(o3, o9)):
        ax2.text(xi - w / 2, a + 3, f"{a:.0f}%", ha="center", color=TEXT2,
                 fontsize=8.5)
        ax2.text(xi + w / 2, b + 3,
                 ("will not launch\n(registers)" if b == 0 else f"{b:.0f}%"),
                 ha="center", va="bottom", color=AMBER if b == 0 else TEXT2,
                 fontsize=8 if b == 0 else 8.5,
                 fontweight="bold" if b == 0 else "normal")
    ax2.set_xticks(x); ax2.set_xticklabels(blocks, color=TEXT2, fontsize=8.5)
    ax2.set_ylim(0, 122); ax2.set_yticks([])
    bare(ax2, keep=("bottom",))
    ax2.legend(frameon=False, fontsize=8.5, labelcolor=TEXT2, loc="upper right")
    ax2.set_title("occupancy vs block size  (halo overhead quoted for 3×3)",
                  color=MUTED, fontsize=8.5, pad=8)
    fig.subplots_adjust(bottom=0.26)
    save(fig, "fig_occupancy")


if __name__ == "__main__":
    fig_tile()
    fig_occupancy()
    fig_frame()
    print("done ->", OUT)
