"""Per-engine busy time and duty cycle from an nsys SQLite export.

    nsys stats --force-export=true --report cuda_gpu_sum <rep>.nsys-rep
    python gpu_span.py <rep>.sqlite <n_frames>

Per engine (kernel / H2D / D2H): sum of op durations, busy = union of the
intervals (overlaps not double-counted), duty = busy / processing window.
The window is first kernel start to last D2H end, so the pedestal-upload
copies that precede any kernel are excluded.

Roofline = tallest engine's busy time per frame. PCIe is full-duplex and the
engines overlap, so the floor is the max, never the sum.
"""
import sqlite3
import sys

Q = {"kernel": "SELECT start,end FROM CUPTI_ACTIVITY_KIND_KERNEL ORDER BY start",
     "H2D": "SELECT start,end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=1 ORDER BY start",
     "D2H": "SELECT start,end FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=2 ORDER BY start"}


def union(rows):
    busy, cs, ce = 0, *rows[0]
    for s, e in rows[1:]:
        if s > ce:
            busy += ce - cs
            cs, ce = s, e
        else:
            ce = max(ce, e)
    return busy + ce - cs


def analyze(sqlite_path, n_frames) -> dict:
    db = sqlite3.connect(str(sqlite_path))
    ops = {k: db.execute(q).fetchall() for k, q in Q.items()}
    lo = ops["kernel"][0][0]
    hi = max(ops["kernel"][-1][1], ops["D2H"][-1][1])
    span = hi - lo

    out = {"n_frames": n_frames, "window_ms": span / 1e6,
           "window_us_per_frame": span / 1e3 / n_frames}
    for k, rows in ops.items():
        rows = [r for r in rows if r[1] > lo]
        tot, busy = sum(e - s for s, e in rows), union(rows)
        out[f"{k}_n"] = len(rows)
        out[f"{k}_sum_ms"] = tot / 1e6
        out[f"{k}_busy_ms"] = busy / 1e6
        out[f"{k}_overlap"] = tot / busy
        out[f"{k}_duty_pct"] = 100 * busy / span
        out[f"{k}_us_per_frame"] = busy / 1e3 / n_frames
    tallest = max(("kernel", "H2D", "D2H"), key=lambda k: out[f"{k}_us_per_frame"])
    out["bottleneck"] = tallest
    out["roofline_us_per_frame"] = out[f"{tallest}_us_per_frame"]
    out["roofline_fps"] = 1e6 / out["roofline_us_per_frame"]
    return out


if __name__ == "__main__":
    N = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    r = analyze(sys.argv[1], N)
    print(f"processing window: {r['window_ms']:.1f} ms / {N} frames = "
          f"{r['window_us_per_frame']:.1f} us/frame")
    for k in ("kernel", "H2D", "D2H"):
        print(f"  {k:6s} sum={r[k + '_sum_ms']:7.2f} ms  busy={r[k + '_busy_ms']:7.2f} ms  "
              f"overlap={r[k + '_overlap']:4.2f}x  duty={r[k + '_duty_pct']:5.1f}%  "
              f"per-frame={r[k + '_us_per_frame']:5.2f} us")
    print(f"  roofline = {r['bottleneck']} at {r['roofline_us_per_frame']:.2f} us/frame "
          f"= {r['roofline_fps']:,.0f} FPS")
