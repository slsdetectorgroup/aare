"""Plot a cpu_vs_gpu.py campaign: throughput and speed-up against bin width.

    plot_cpu_vs_gpu.py [CSV] [--out DIR] [--kernel window|direct]

CSV is the file written by `cpu_vs_gpu.py --csv` (default: the A100 campaign
of 2026-11-05). Writes two PNGs next to it, or into --out:

  throughput_vs_binwidth.png  frames/s of the CPU (float64 input, the only
                              type its binding takes) and of the GPU for
                              each input type, with the PCIe copy limit of
                              each GPU input type
  speedup_vs_binwidth.png     GPU over CPU, one line per GPU input type

Times are for the whole pipeline on both sides: on the GPU the fill (frames copied in, kernel) plus one values() (the
histogram copied back). The readout is paid once per run, so it weighs more
in a short run. The PCIe lines are the limit of the copy in alone.

A CSV with a gpu_kernel column holds runs of both fill kernels: only the
--kernel ones are plotted (default window) and the PNG names end in _window
or _direct. Older CSVs have no such column; their runs are the direct kernel.

The accumulator layout is not in the CSV. The current code is pixel-major
only, but older runs may be bin-major: a folder name with "binMajor" (any
case) marks them, as does --layout. Everything else is pixel-major.
"""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter, NullFormatter  # noqa: E402

HERE = Path(__file__).resolve().parent
DEFAULT_CSV = HERE / "results/A100/2026-11-11_window/pixelMajor/runs.csv"

# Host-to-device bandwidth on the A100 nodes: the float64 fill is copy-bound
# at 48.2 us for 1.28 MB per frame (u16: 12.0 us, f32: 24.0 us in the nsys
# probes, the same rate).
PCIE_GBPS = 26.6
NPIX = 400 * 400

# Okabe-Ito colours (colour-blind safe), plus a marker per series so the
# lines stay distinct in greyscale.
STYLE = {
    "cpu": dict(color="#000000", marker="D", label="CPU, float64 frames (30 threads)"),
    "u16": dict(color="#0072B2", marker="o", label="GPU, uint16 frames"),
    "f32": dict(color="#E69F00", marker="s", label="GPU, float32 frames"),
    "f64": dict(color="#009E73", marker="^", label="GPU, float64 frames"),
}
BYTES = {"u16": 2, "f32": 4, "f64": 8}

ap = argparse.ArgumentParser()
ap.add_argument("csv", type=Path, nargs="?", default=DEFAULT_CSV)
ap.add_argument("--out", type=Path, help="output directory (default: the CSV's)")
ap.add_argument("--layout", choices=["bin-major", "pixel-major"],
                help="accumulator layout (default: bin-major if the CSV's folder name "
                     "says binMajor, else pixel-major)")
ap.add_argument("--kernel", choices=["window", "direct"], default="window",
                help="fill kernel to plot when the CSV has both (default: window)")
args = ap.parse_args()
out = args.out or args.csv.parent
out.mkdir(parents=True, exist_ok=True)

# ---- data --------------------------------------------------------------------
rows = list(csv.DictReader(args.csv.open()))
has_kernel = "gpu_kernel" in rows[0]
kernel = args.kernel if has_kernel else "direct"
rows = [r for r in rows if r.get("gpu_kernel", "direct") == kernel]
if not rows:
    raise SystemExit(f"no {kernel} runs in {args.csv}")
suffix = f"_{kernel}" if has_kernel else ""
n_frames = int(rows[0]["n_frames"])
range_adu = float(rows[0]["range_adu"])
node = rows[0]["node"]
folder = args.csv.resolve().parent.name.lower()
layout = args.layout or ("bin-major" if "binmajor" in folder else "pixel-major")

cpu = {}                      # adu_per_bin -> CPU seconds
gpu = defaultdict(dict)       # input -> adu_per_bin -> GPU seconds
for r in rows:
    w = range_adu / int(r["n_bins"])
    cpu[w] = float(r["cpu_s"])  # one CPU run per bin count, repeated per input
    gpu[r["gpu_input"]][w] = float(r["gpu_fill_s"]) + float(r["gpu_values_s"])

widths = sorted(cpu, reverse=True)  # wide bins first


def xaxis(ax):
    """Bin width on a log2 axis, narrower bins to the right, bin count on top."""
    ax.set_xscale("log", base=2)
    ax.set_xlim(max(widths) * 1.3, min(widths) / 1.3)
    ax.set_xticks(widths)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("Bin width (ADU per bin), narrower bins to the right")
    inv = lambda v: range_adu / np.maximum(v, 1e-9)  # noqa: E731  (w <-> n_bins)
    top = ax.secondary_xaxis("top", functions=(inv, inv))
    top.set_xticks([range_adu / w for w in widths])
    top.xaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
    top.xaxis.set_minor_formatter(NullFormatter())
    top.set_xlabel(f"Number of bins (over {range_adu:g} ADU)")
    ax.grid(True, which="major", alpha=0.3)


SUBTITLE = (f"A100 vs EPYC 7713 (30 threads, float64 input, {node})  ·  {layout}, {kernel} kernel  ·  "
            f"{n_frames:,} frames of 400×400")


NOTE = "GPU time: frames copied in + fill kernel + one values() (histogram copied back)"


def titles(fig, ax, title):
    """Short bold title on the figure, two grey lines of context under it,
    kept clear of the top axis label."""
    fig.suptitle(title, fontsize=12, fontweight="bold")
    ax.set_title(f"{SUBTITLE}\n{NOTE}", fontsize=8.5, color="0.4", pad=14)

# ---- throughput ----------------------------------------------------------------
fig, ax = plt.subplots(figsize=(8, 5.6), layout="constrained")
ax.plot(widths, [n_frames / cpu[w] for w in widths], lw=2, ms=7, **STYLE["cpu"])
for inp in ("u16", "f32", "f64"):
    ws = sorted(gpu[inp], reverse=True)
    st = STYLE[inp]
    ax.plot(ws, [n_frames / gpu[inp][w] for w in ws], lw=2, ms=7, **st)
    limit = PCIE_GBPS * 1e9 / (NPIX * BYTES[inp])
    ax.axhline(limit, color=st["color"], ls="--", lw=1, alpha=0.7)
    ax.annotate(f"PCIe limit of the copy in alone, {inp}: {limit / 1e3:.0f}k frames/s",
                (max(widths) * 1.25, limit), xytext=(0, 3), textcoords="offset points",
                color=st["color"], fontsize=8, va="bottom")
ax.set_yscale("log")
ax.set_ylim(2e3, 1.5e5)
ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v / 1e3:g}k"))
ax.set_ylabel("Frames per second, whole pipeline (log scale)")
xaxis(ax)
titles(fig, ax, "Throughput of the whole pipeline, GPU vs CPU")
ax.legend(loc="lower left", fontsize=9, framealpha=0.9)
p1 = out / f"throughput_vs_binwidth{suffix}.png"
fig.savefig(p1, dpi=200)
plt.close(fig)

# ---- speed-up ------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(8, 5.6), layout="constrained")
# uint16 labels above its points, float32 below: the two lines meet at 1-2 ADU.
LABEL_DY = {"u16": 8, "f32": -16, "f64": 8}
for inp in ("u16", "f32", "f64"):
    ws = sorted(gpu[inp], reverse=True)
    s = [cpu[w] / gpu[inp][w] for w in ws]
    ax.plot(ws, s, lw=2, ms=7, **STYLE[inp])
    for w, v in zip(ws, s):
        ax.annotate(f"{v:.1f}×", (w, v), xytext=(0, LABEL_DY[inp]), textcoords="offset points",
                    ha="center", fontsize=8, color=STYLE[inp]["color"])
ax.set_ylim(0, max(cpu[w] / gpu[i][w] for i in gpu for w in gpu[i]) * 1.15)
ax.set_ylabel("Speed-up over the CPU (CPU time / GPU time)")
xaxis(ax)
titles(fig, ax, "GPU speed-up over the CPU, whole pipeline")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.13), ncol=3, fontsize=9,
          frameon=False)
p2 = out / f"speedup_vs_binwidth{suffix}.png"
fig.savefig(p2, dpi=200)
plt.close(fig)

print(f"wrote {p1}\nwrote {p2}")
