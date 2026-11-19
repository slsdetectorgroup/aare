"""Plot how the batch size changes the GPU fill, from an nsys batch sweep.

    plot_batch.py PROBES_CSV [--out DIR]

PROBES_CSV is the probes.csv of run_probes.py runs that differ in --batch
(same input type and kernel, a few --bins). Two panels against the batch
size, both in µs per frame:

  left   busy time of the fill kernel, one line per bin width, and of the
         copy in (host to GPU): a kernel under the copy line is hidden
  right  fill time per frame, first copy start to last kernel end: what
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

FOOTNOTE = (
    "* Busy time per frame = (sum of the durations of all fill kernels) / number of frames; "
    "for the copy line, the same with the frame copies to the GPU.\n"
    "** Fill time per frame = (end of the last kernel - start of the first copy) / number of frames: the real rate.\n"
    "   Copies and kernels run at the same time, so it follows the slower of the two.")

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

# Plots on top; legend and footnote in their own strip below.
fig = plt.figure(figsize=(10.5, 5.75), layout="constrained")
top, bottom = fig.subfigures(2, 1, height_ratios=[4.8, 0.95])
ax1, ax2 = top.subplots(1, 2, sharey=True)
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
ax1.set_ylabel("Busy time per frame (µs) *")
ax2.set_ylabel("Fill time per frame (µs) **")
ax2.yaxis.set_tick_params(labelleft=True)
ax1.set_title("Busy time of the fill kernel and of the copy", fontsize=10)
ax2.set_title("Fill time, what the pipeline delivers", fontsize=10)
top.suptitle(f"Effect of the batch size on the GPU fill ({DTYPE_NAME.get(r0['dtype'], r0['dtype'])} "
             f"frames, {r0['kernel']} kernel"
             f"{', pedestal tracking' if r0.get('tracking_n_sigma') else ''})",
             fontsize=12, fontweight="bold")
handles, labels = ax1.get_legend_handles_labels()
bottom.legend(handles, labels, loc="upper center", ncol=len(labels), fontsize=9, frameon=False)
bottom.text(0.5, 0.02, f"nsys, {int(r0['n_frames']):,} frames per run.\n" + FOOTNOTE,
            ha="center", va="bottom", multialignment="left", fontsize=8.5, color="0.25")
p = out / "batch_size.png"
fig.savefig(p, dpi=200, bbox_inches="tight")
print(f"wrote {p}")
