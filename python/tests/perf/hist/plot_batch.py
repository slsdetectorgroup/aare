"""Plot how the batch size changes the GPU fill, from an nsys batch sweep.

    plot_batch.py PROBES_CSV [--out DIR]

PROBES_CSV is the probes.csv of run_probes.py runs that differ in --batch
(same input type and kernel, a few --bins). Two panels against the batch
size, both in µs per frame:

  left   busy time of the fill kernel, one line per bin width, and of the
         copy in (host to GPU): a kernel under the copy line is hidden
  right  fill time per frame, first kernel start to last kernel end: what
         the pipeline actually delivers, including gaps between batches

Writes batch_size.png next to the CSV, or into --out.
"""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter, NullFormatter  # noqa: E402

# Okabe-Ito colours and one marker per bin width; the copy is black, dashed.
WIDTH_STYLE = [dict(color="#0072B2", marker="o"), dict(color="#E69F00", marker="s"),
               dict(color="#009E73", marker="^"), dict(color="#CC79A7", marker="v")]
DTYPE_NAME = {"u16": "uint16", "f32": "float32", "f64": "float64"}

ap = argparse.ArgumentParser()
ap.add_argument("csv", type=Path)
ap.add_argument("--out", type=Path, help="output directory (default: the CSV's)")
args = ap.parse_args()
out = args.out or args.csv.parent

rows = [r for r in csv.DictReader(args.csv.open()) if r.get("bin_width_adu")]
if not rows:
    raise SystemExit(f"no sweep rows (with bin_width_adu) in {args.csv}")
kernel = defaultdict(dict)  # width -> batch -> us/frame
fill = defaultdict(dict)
copy = defaultdict(list)    # batch -> [us/frame]
for r in rows:
    w, b = float(r["bin_width_adu"]), int(r["batch"])
    kernel[w][b] = float(r["kernel_us_per_frame"])
    fill[w][b] = float(r["window_us_per_frame"])
    copy[b].append(float(r["H2D_us_per_frame"]))
widths = sorted(kernel, reverse=True)
batches = sorted(copy)
copy_line = [sum(copy[b]) / len(copy[b]) for b in batches]
r0 = rows[0]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10.5, 4.8), layout="constrained", sharey=True)
for i, w in enumerate(widths):
    st = WIDTH_STYLE[i % len(WIDTH_STYLE)]
    bs = sorted(kernel[w])
    lab = f"{w:g} ADU per bin"
    ax1.plot(bs, [kernel[w][b] for b in bs], lw=2, ms=6, label=lab, **st)
    ax2.plot(bs, [fill[w][b] for b in bs], lw=2, ms=6, label=lab, **st)
for ax in (ax1, ax2):
    ax.plot(batches, copy_line, color="#000000", ls="--", marker="D", lw=2, ms=5,
            label="Copy in (host to GPU)")
    ax.set_xscale("log", base=2)
    ax.set_xticks(batches)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("Batch size (frames per copy and kernel launch)")
    ax.set_ylim(0, None)
    ax.grid(True, alpha=0.3)
ax1.set_ylabel("Time per frame (µs)")
ax1.set_title("Busy time of the fill kernel and of the copy", fontsize=10)
ax2.set_title("Fill time, what the pipeline delivers", fontsize=10)
fig.suptitle(f"Effect of the batch size on the GPU fill ({DTYPE_NAME.get(r0['dtype'], r0['dtype'])} "
             f"frames, {r0['kernel']} kernel)", fontsize=12, fontweight="bold")
handles, labels = ax1.get_legend_handles_labels()
leg = fig.legend(handles, labels, loc="outside lower center", ncol=len(labels), fontsize=9,
                 frameon=False, title=f"nsys, {int(r0['n_frames']):,} frames per run", title_fontsize=8.5)
leg.get_title().set_color("0.4")
p = out / "batch_size.png"
fig.savefig(p, dpi=200, bbox_inches="tight")
print(f"wrote {p}")
