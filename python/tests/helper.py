"""Helpers for the CPU-vs-CUDA ClusterFinder mismatch analysis.

Pure, parameterised versions of the functions that used to live inline in the
notebook's forensic-view cell, plus the reusable scan + plot routines so the
notebook cells stay thin.  Import in the notebook with::

    from helper import (centers, only_sets, footprint_mask, shift_dist,
                        train_pedestal, scan_mismatches, plot_masked_mismatch)

None of these rely on notebook globals — cluster geometry (rx, ry) and the
frame shape (rows, cols) are passed in explicitly.
"""

import numpy as np
import matplotlib.pyplot as plt
import boost_histogram as bh

# --------------------------------------------------------------------------- #
#  Pure helpers
# --------------------------------------------------------------------------- #
def print_pinning_budget(rows, cols, dtype=np.uint16, headroom_gb=4.0):
    """
    Print system RAM stats and estimate the maximum number of frames
    that can be safely registered with cudaHostRegister / register_input_buffer.

    Parameters
    ----------
    rows, cols : int      Detector frame dimensions.
    dtype      : np.dtype Frame element type (default uint16 = 2 bytes/pixel).
    headroom_gb: float    RAM to keep free for OS + CUDA context (default 4 GB).
    """
    # /proc/meminfo is always available on Linux — no extra dependency needed
    meminfo = {}
    with open('/proc/meminfo') as f:
        for line in f:
            key, val = line.split(':')
            meminfo[key.strip()] = int(val.split()[0]) * 1024  # kB → bytes

    total_bytes     = meminfo['MemTotal']
    available_bytes = meminfo['MemAvailable']   # free + reclaimable cache
    safe_bytes      = max(0, available_bytes - int(headroom_gb * 1024**3))

    frame_bytes = rows * cols * np.dtype(dtype).itemsize
    max_frames  = safe_bytes // frame_bytes

    GiB = 1024**3
    print("── System RAM ──────────────────────────────────────────")
    print(f"  Total RAM           : {total_bytes/GiB:.1f} GiB")
    print(f"  Currently available : {available_bytes/GiB:.1f} GiB  "
          f"(free + reclaimable cache)")
    print(f"  Reserved headroom   : {headroom_gb:.1f} GiB  "
          f"(OS + CUDA context)")
    print(f"  Safe pinning budget : {safe_bytes/GiB:.1f} GiB")
    print()
    print("── Frame layout ────────────────────────────────────────")
    print(f"  Frame size          : {rows} × {cols} × "
          f"{np.dtype(dtype).itemsize} B = {frame_bytes/1024:.1f} kB")
    print()
    print("── Pinning estimate ────────────────────────────────────")
    print(f"  Max frames pinnable : {max_frames:,}  "
          f"({max_frames * frame_bytes / GiB:.1f} GiB)")
    print()
    print("  Note: no swap on this machine — exceeding available RAM")
    print("  will trigger the OOM killer. Stay within the budget.")

def centers(cv):
    """Set of ``(x, y)`` integer cluster centres from a ClusterVector."""
    if cv.size == 0:
        return set()
    a = np.asarray(cv)
    return {(int(x), int(y)) for x, y in zip(a["x"], a["y"])}


def only_sets(cpu_c, cu_c, tol=1):
    """CPU-only / CUDA-only centres, ignoring ``<=tol`` px 'shifted' matches.

    ``tol=0`` returns the exact set difference (shifted twins stay counted as
    mismatches); ``tol=1`` drops any mismatch that has a counterpart in the
    other finder's 8-neighbourhood.
    """
    def near(p, other):
        x, y = p
        return any((x + dx, y + dy) in other
                   for dx in range(-tol, tol + 1)
                   for dy in range(-tol, tol + 1))

    cpu_only = {p for p in cpu_c - cu_c if not near(p, cu_c)}
    cu_only = {p for p in cu_c - cpu_c if not near(p, cpu_c)}
    return cpu_only, cu_only


def footprint_mask(cs, shape, rx, ry):
    """Boolean map: True where a pixel lies in some cluster's footprint.

    Each centre paints a ``(2*ry+1) x (2*rx+1)`` box, clipped at the borders.
    """
    m = np.zeros(shape, bool)
    for (cx, cy) in cs:
        m[max(0, cy - ry):cy + ry + 1, max(0, cx - rx):cx + rx + 1] = True
    return m


def shift_dist(p, other, R=4):
    """Chebyshev distance from ``p`` to the nearest member of ``other``.

    Rings are searched inner-to-outer so the first hit is the nearest.
    Returns ``-1`` if nothing lies within ``R`` px.
    """
    x, y = p
    for r in range(1, R + 1):
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                if max(abs(dx), abs(dy)) == r and (x + dx, y + dy) in other:
                    return r
    return -1


# --------------------------------------------------------------------------- #
#  Reusable scan + plot
# --------------------------------------------------------------------------- #
def train_pedestal(finders, f, n_frames, seek=0):
    """Push the first ``n_frames`` frames of ``f`` into every finder given."""
    f.seek(seek)
    for _ in range(n_frames):
        img = f.read_frame().copy()
        for cf in finders:
            cf.push_pedestal_frame(img)


def scan_mismatches(cf_cpu, cf_cuda, data, rx, ry,
                    scan_count=1000, n_show=8, tol=0):
    """Run both finders over ``scan_count`` frames sampled across ``data``.

    Before every frame it snapshots the pedestal each finder will DECIDE with
    (CPU: host mean/rms; CUDA: device mean/rms on stream 0), so a later
    recompute uses the exact decision-time baseline.

    ``tol`` is forwarded to :func:`only_sets` when scoring a frame; ``tol=0``
    keeps shifted twins as mismatches.  Returns ``(show, totals)`` where
    ``show`` is the ``n_show`` frames with the most mismatches, each a dict
    with ``fid``, ``score``, ``cpu_c``, ``cu_c``, ``mism`` and the four
    pedestal arrays ``ped_cpu/noise_cpu/ped_cu/noise_cu``.
    """
    show = []
    tot_cpu_only = tot_cu_only = 0
    for fid in np.linspace(0, len(data) - 1, scan_count, dtype=int):
        snap = dict(ped_cpu=np.asarray(cf_cpu.pedestal).copy(),
                    noise_cpu=np.asarray(cf_cpu.noise).copy(),
                    ped_cu=np.asarray(cf_cuda.device_pedestal(0)).copy(),
                    noise_cu=np.asarray(cf_cuda.device_noise(0)).copy())
        cf_cpu.find_clusters(data[fid])
        cpu_c = centers(cf_cpu.steal_clusters(realloc_same_capacity=True))
        cf_cuda.find_clusters(data[fid])
        cu_c = centers(cf_cuda.steal_clusters(realloc_same_capacity=True))

        cpu_only, cu_only = only_sets(cpu_c, cu_c, tol=tol)
        tot_cpu_only += len(cpu_only)
        tot_cu_only += len(cu_only)
        mism = cpu_only | cu_only
        if not mism:
            continue
        if len(show) < n_show or len(mism) > show[-1]["score"]:
            show.append(dict(score=len(mism), fid=int(fid),
                             cpu_c=cpu_c, cu_c=cu_c, mism=mism, **snap))
            show.sort(key=lambda e: -e["score"])
            del show[n_show:]
    return show, dict(cpu_only=tot_cpu_only, cu_only=tot_cu_only)


def compare_finders(finders, data, scan_count=1000, n_bins=200, e_range=(-2, 4000)):
    """Run several finders over the same frames and score pairwise agreement.

    ``finders`` is a dict ``{name: finder}``; every finder must already be
    trained on the SAME pedestal frames.  Each is run over the same
    ``scan_count`` frames sampled across ``data`` (identical ``find_clusters``/
    ``steal_clusters`` API for CPU, frozen-CPU and CUDA).  In the same pass it
    accumulates a per-finder cluster-energy histogram (from ``cv.sum()``), so no
    extra scan is needed to draw the spectra.

    Returns ``(totals, pairs, frames_scanned, hists)``:
      * ``totals[name]``          total clusters found by that finder
      * ``pairs[(a, b)]``         dict with ``a_only``/``b_only``/``mismatch``
                                  summed over frames (exact, tol=0)
      * ``hists[name]``           boost ``Histogram`` of cluster energies
    Companions :func:`print_comparison` and :func:`plot_spectra` render these.
    """
    names = list(finders)
    totals = {n: 0 for n in names}
    hists = {n: bh.Histogram(bh.axis.Regular(n_bins, *e_range)) for n in names}
    pairs = {}
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            pairs[(a, b)] = dict(a_only=0, b_only=0, mismatch=0)

    fids = np.linspace(0, len(data) - 1, scan_count, dtype=int)
    for fid in fids:
        cs = {}
        for n, cf in finders.items():
            cf.find_clusters(data[fid])
            cv = cf.steal_clusters(realloc_same_capacity=True)
            cs[n] = centers(cv)
            totals[n] += len(cs[n])
            if cv.size:
                hists[n].fill(np.asarray(cv.sum()).ravel())
        for (a, b), acc in pairs.items():
            a_only, b_only = only_sets(cs[a], cs[b], tol=0)
            acc["a_only"] += len(a_only)
            acc["b_only"] += len(b_only)
            acc["mismatch"] += len(a_only) + len(b_only)
    return totals, pairs, len(fids), hists


def plot_spectra(hists, totals=None, title="Cluster energy spectrum"):
    """Overlay per-finder cluster-energy spectra with a ratio panel.

    ``hists`` is the ``{name: Histogram}`` returned by :func:`compare_finders`;
    the first finder is the reference for the ratio panel.  Returns the Figure.
    """
    names = list(hists)
    ref = names[0]
    edges = hists[ref].axes[0].edges
    vals = {n: hists[n].values() for n in names}

    fig, (ax_spec, ax_ratio) = plt.subplots(
        2, 1, figsize=(8, 6), sharex=True,
        gridspec_kw={"height_ratios": [3, 1]})

    styles = ["-", "--", "-.", ":"]
    for i, n in enumerate(names):
        lbl = n if totals is None else f"{n} ({totals[n]:,} clusters)"
        ax_spec.stairs(vals[n], edges, label=lbl, linestyle=styles[i % len(styles)])
    ax_spec.set_ylabel("Counts")
    ax_spec.set_title(title)
    ax_spec.legend()
    ax_spec.grid(alpha=0.2)

    with np.errstate(divide="ignore", invalid="ignore"):
        for i, n in enumerate(names[1:], start=1):
            ratio = np.where(vals[ref] > 0, vals[n] / vals[ref], np.nan)
            ax_ratio.stairs(ratio, edges, label=f"{n} / {ref}",
                            color=f"C{i}", linestyle=styles[i % len(styles)])
    ax_ratio.axhline(1.0, color="gray", linewidth=0.5)
    ax_ratio.set_ylabel(f"/ {ref}")
    ax_ratio.set_xlabel("Energy [ADU]")
    ax_ratio.set_ylim(0.5, 2.0)
    ax_ratio.legend(fontsize=8)
    ax_ratio.grid(alpha=0.3)

    plt.tight_layout()
    plt.show()
    return fig


def print_comparison(totals, pairs, frames_scanned):
    """Pretty-print the output of :func:`compare_finders`."""
    print(f"Scanned {frames_scanned} frames\n")
    print("Total clusters per finder:")
    for n, t in totals.items():
        print(f"  {n:<16} {t:>12,}")
    print("\nPairwise exact mismatches (tol=0):")
    print(f"  {'pair':<28} {'A-only':>10} {'B-only':>10} {'total':>10}")
    for (a, b), acc in pairs.items():
        ref = max(totals[a], totals[b], 1)
        pct = 100.0 * acc["mismatch"] / ref
        print(f"  {a+' vs '+b:<28} {acc['a_only']:>10,} "
              f"{acc['b_only']:>10,} {acc['mismatch']:>10,}  ({pct:.4f}%)")


def walkthrough(show, data, rx, ry, rows, cols, n_sigma, pick=0,
                labels=('A', 'B')):
    """Manual Test1/Test3 recompute of the strongest residual mismatch.

    Dissects ``show[pick]`` (from :func:`scan_mismatches`, run as ``(a, b)``)
    under each finder's decision-time snapshot pedestal — ``a`` = ``ped_cpu`` /
    ``noise_cpu``, ``b`` = ``ped_cu`` / ``noise_cu`` — and prints the raw and
    pedestal-subtracted window plus the accept/reject each finder reaches.  With
    the double/double build the recompute reproduces the kernel exactly, so a
    lone surviving mismatch (e.g. the single 7x7 residual) is fully explained:
    the test that flips (Test1/Test3) and the pedestal gap name the cause.
    """
    if not show:
        print("No residual mismatches to walk through.")
        return
    e = show[pick]
    sx, sy = 2 * rx + 1, 2 * ry + 1
    c3 = np.sqrt(sx * sy)
    frame = data[e['fid']].astype(np.float64)
    sub_a = frame - e['ped_cpu']
    sub_b = frame - e['ped_cu']

    # strongest mismatch pixel whose full window stays inside the frame
    cand = [p for p in e['mism']
            if rx <= p[0] < cols - rx and ry <= p[1] < rows - ry]
    if not cand:
        print(f"frame {e['fid']}: all mismatches on the border — pick another.")
        return
    X0, Y0 = max(cand, key=lambda p: max(sub_a[p[1], p[0]], sub_b[p[1], p[0]]))

    def evaluate(mean, rms_img):
        sig = (frame[Y0 - ry:Y0 + ry + 1, X0 - rx:X0 + rx + 1]
               - mean[Y0 - ry:Y0 + ry + 1, X0 - rx:X0 + rx + 1])
        value = frame[Y0, X0] - mean[Y0, X0]
        rms = rms_img[Y0, X0]
        thr1, thr3 = n_sigma * rms, c3 * n_sigma * rms
        mx, total = sig.max(), sig.sum()
        localmax = bool(value >= mx)
        t1, t3 = bool(mx > thr1), bool(total > thr3)
        accept = bool(value >= -thr1) and localmax and (t1 or t3)
        return dict(sig=sig, value=value, rms=rms, thr1=thr1, thr3=thr3,
                    mx=mx, total=total, localmax=localmax, t1=t1, t3=t3,
                    accept=accept)

    la, lb = labels
    owner = la if (X0, Y0) in e['cpu_c'] else lb
    print(f"frame {e['fid']}   centre (x={X0}, y={Y0})   accepted only by {owner}")
    print("raw window (ADU):")
    print(np.array2string(frame[Y0 - ry:Y0 + ry + 1,
                                X0 - rx:X0 + rx + 1].astype(int)))
    print()

    ra = evaluate(e['ped_cpu'], e['noise_cpu'])
    rb = evaluate(e['ped_cu'], e['noise_cu'])

    def detail(tag, r):
        print(f"--- {tag} ---")
        print("  subtracted window:")
        print("   ", np.array2string(r['sig'], precision=1, prefix='    '))
        print(f"  centre value = {float(r['value']):8.2f}   "
              f"(local max? {r['localmax']})")
        print(f"  max          = {float(r['mx']):8.2f}      "
              f"total = {float(r['total']):8.2f}")
        print(f"  rms(centre)  = {float(r['rms']):8.3f}")
        print(f"  Test1: max   > {n_sigma}*rms      = {float(r['thr1']):8.2f} -> {r['t1']}")
        print(f"  Test3: total > {c3:.0f}*{n_sigma}*rms = {float(r['thr3']):8.2f} -> {r['t3']}")
        print(f"  ACCEPT = {r['accept']}")
        print()

    detail(la, ra)
    detail(lb, rb)
    print(f"RESULT:  {la} = {'ACCEPT' if ra['accept'] else 'reject'} ,  "
          f"{lb} = {'ACCEPT' if rb['accept'] else 'reject'}")
    gap = abs(float(e['ped_cpu'][Y0, X0] - e['ped_cu'][Y0, X0]))
    rgap = abs(float(e['noise_cpu'][Y0, X0] - e['noise_cu'][Y0, X0]))
    print(f"pedestal mean gap @centre = {gap:.4f} ADU;   rms gap = {rgap:.4f}")
    if ra['accept'] == rb['accept']:
        print("NOTE: recompute agrees for this centre pixel — the split is FP "
              "rounding right at threshold or a window-neighbour effect; "
              "try another pick.")


def plot_masked_mismatch(show, data, rx, ry, rows, cols,
                         zoom=30, show_vals=True):
    """Side-by-side masked view of each frame in ``show``.

    Cluster pixels are coloured by pedestal-subtracted value (value printed
    when ``show_vals``); non-cluster pixels are white; a red dot marks every
    cluster centre.  The cut is centred on the strongest mismatch pixel of the
    frame.  No box, no tolerance discarding — a shifted twin in the other
    finder stays visible.  Returns the Figure.
    """
    cmap = plt.cm.viridis.copy()
    cmap.set_bad("white")
    fig, axes = plt.subplots(len(show), 2, figsize=(11, 5.3 * len(show)),
                             squeeze=False)
    for r, e in enumerate(show):
        fid = e["fid"]
        sub_cpu = data[fid].astype(np.float64) - e["ped_cpu"]
        sub_cu = data[fid].astype(np.float64) - e["ped_cu"]
        X0, Y0 = max(e["mism"],
                     key=lambda p: max(sub_cpu[p[1], p[0]], sub_cu[p[1], p[0]]))
        half = zoom // 2
        r0, r1 = max(0, Y0 - half), min(rows, Y0 + half)
        c0, c1 = max(0, X0 - half), min(cols, X0 + half)
        win_cpu, win_cu = sub_cpu[r0:r1, c0:c1], sub_cu[r0:r1, c0:c1]
        mask_cpu = footprint_mask(e["cpu_c"], sub_cpu.shape, rx, ry)[r0:r1, c0:c1]
        mask_cu = footprint_mask(e["cu_c"], sub_cu.shape, rx, ry)[r0:r1, c0:c1]
        union = mask_cpu | mask_cu
        vmax = max(np.percentile(np.concatenate([win_cpu[union], win_cu[union]]),
                                 99) if union.any() else 50.0, 50.0)

        for ax, win, mask, cs, name in [
                (axes[r][0], win_cpu, mask_cpu, e["cpu_c"], "CPU"),
                (axes[r][1], win_cu, mask_cu, e["cu_c"], "CUDA")]:
            ax.imshow(np.ma.masked_where(~mask, win), cmap=cmap, vmin=0,
                      vmax=vmax, interpolation="nearest")
            for (cx, cy) in cs:
                if c0 <= cx < c1 and r0 <= cy < r1:
                    ax.plot(cx - c0, cy - r0, ".", color="red", ms=7)
            if show_vals:
                for i in range(win.shape[0]):
                    for j in range(win.shape[1]):
                        if mask[i, j]:
                            ax.text(j, i, f"{win[i, j]:.0f}", ha="center",
                                    va="center", fontsize=6,
                                    color="white" if win[i, j] < 0.55 * vmax
                                    else "black")
            ax.set_title(f"frame {fid} — {name}: {len(cs)} clusters", fontsize=10)
            ax.set_xticks([])
            ax.set_yticks([])
        axes[r][0].set_ylabel(f"{e['score']} mismatches\nzoom @ ({X0},{Y0})",
                              fontsize=9)
    plt.tight_layout()
    plt.show()
    return fig
