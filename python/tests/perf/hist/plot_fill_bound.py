"""Plot whether the GPU fill is limited by the copy in or by the fill kernel.

    plot_fill_bound.py PROBES_CSV [--out DIR]

PROBES_CSV is the probes.csv of a run_probes.py sweep (--dtypes ... --bins
... --kernel window direct --range XMIN XMAX). For each input type, one panel
with the time per frame that the copy engine (host to GPU) and the fill kernel
are busy, against the bin width. Copy and kernel overlap, so the fill runs at
the speed of the slower one: where a kernel line is under the copy line, the
fill is copy-bound. The readout (values()) is left out: it runs once, after
the last kernel.

Writes fill_bound.png next to the CSV, or into --out.
"""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter, NullFormatter  # noqa: E402

DTYPE_NAME = {"u16": "uint16 frames", "f32": "float32 frames", "f64": "float64 frames"}
# Okabe-Ito colours, plus a marker and a line style per series so the lines
# stay distinct in greyscale.
STYLE = {
    "H2D": dict(color="#000000", marker="D", ls="--", label="Copy in (host to GPU)"),
    "window": dict(color="#0072B2", marker="o", ls="-", label="Window kernel (fill_window)"),
    "direct": dict(color="#D55E00", marker="s", ls="-", label="Direct kernel (fill_direct)"),
}

ap = argparse.ArgumentParser()
ap.add_argument("csv", type=Path)
ap.add_argument("--out", type=Path, help="output directory (default: the CSV's)")
args = ap.parse_args()
out = args.out or args.csv.parent

rows = [r for r in csv.DictReader(args.csv.open()) if r.get("bin_width_adu")]
if not rows:
    raise SystemExit(f"no sweep rows (with bin_width_adu) in {args.csv}")

kern = defaultdict(dict)                    # (dtype, kernel) -> width -> us/frame
copy = defaultdict(lambda: defaultdict(list))  # dtype -> width -> [us/frame]
for r in rows:
    w = float(r["bin_width_adu"])
    kern[(r["dtype"], r["kernel"])][w] = float(r["kernel_us_per_frame"])
    copy[r["dtype"]][w].append(float(r["H2D_us_per_frame"]))
dtypes = [d for d in ("u16", "f32", "f64") if d in copy]
r0 = rows[0]

fig, axes = plt.subplots(1, len(dtypes), figsize=(4.6 * len(dtypes), 4.8),
                         layout="constrained", squeeze=False)
for ax, d in zip(axes[0], dtypes):
    ws = sorted(copy[d], reverse=True)
    ax.plot(ws, [sum(copy[d][w]) / len(copy[d][w]) for w in ws], lw=2, ms=6, **STYLE["H2D"])
    for k in ("window", "direct"):
        if (d, k) in kern:
            kw = sorted(kern[(d, k)], reverse=True)
            ax.plot(kw, [kern[(d, k)][w] for w in kw], lw=2, ms=6, **STYLE[k])
    ax.set_xscale("log", base=2)
    ax.set_xlim(max(ws) * 1.3, min(ws) / 1.3)
    ax.set_xticks(ws)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("Bin width (ADU per bin), narrower to the right")
    ax.set_ylim(0, None)
    ax.set_title(DTYPE_NAME[d], fontsize=10)
    ax.grid(True, alpha=0.3)
axes[0][0].set_ylabel("Busy time per frame (µs)")
fig.suptitle("Is the GPU fill limited by the copy in or by the kernel?",
             fontsize=12, fontweight="bold")
handles, labels = axes[0][0].get_legend_handles_labels()
leg = fig.legend(handles, labels, loc="outside lower center", ncol=3, fontsize=9, frameon=False,
                 title=f"nsys, batch {r0['batch']}, {int(r0['n_frames']):,} frames per run. "
                       "Copy and kernel overlap, so the slower one sets the fill speed: "
                       "a kernel under the copy line means copy-bound.",
                 title_fontsize=8.5)
leg.get_title().set_color("0.4")
p = out / "fill_bound.png"
fig.savefig(p, dpi=200, bbox_inches="tight")
print(f"wrote {p}")
