"""Dump one Test3 divergence site in full, for the annex figure.

Re-runs both finders up to the target frame and, at that frame, records the
3x3 neighbourhood as each model saw it: raw ADU, the pedestal mean each model
was reading, the resulting pedestal-subtracted values, and both test statistics
against their thresholds.

The site is the first frame at which a Test3 flip creates a cluster in the
frozen finder that the serial finder does not produce.

Writes branch_site.json next to itself.

REQUIRES a build with branch tracing on, which is OFF by default so the
shipped path carries no per-pixel store:

    sed -i 's/#define AARE_BRANCH_TRACE 0/#define AARE_BRANCH_TRACE 1/' \
        include/aare/ClusterFinder.hpp include/aare/ClusterFinderFrozen.hpp
    cmake --build build -j8

Set it back to 0 afterwards. Without it branch_map is all-5 and this
script reports no divergence at all.
"""
import sys, json
sys.path.append('/home/ferjao_k/aare/build')
sys.path.append('/home/ferjao_k/aare/python/tests')

from pathlib import Path
import numpy as np
from aare import File, ClusterFinder, ClusterFinderFrozen

OUT = Path(__file__).resolve().parent
BASE = Path('/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/'
            'process/xrf/')

N_PED, N_SIGMA = 1000, 5
CLUSTER, IMG, CAP = (3, 3), (400, 400), 50_000
TARGET_FRAME, TY, TX = 203, 125, 245          # from branch_trace.json
C3 = np.sqrt(9.0)                             # sqrt(ClusterSizeX * ClusterSizeY)

f = File(BASE / 'Cu_factor_10_data_master_0.json')
pd = File(BASE / 'Cu_factor_10_pedestal_master_0.json')

cf_cpu = ClusterFinder(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)
cf_frz = ClusterFinderFrozen(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)

pd.seek(0)
for _ in range(N_PED):
    img = pd.read_frame().copy()
    cf_cpu.push_pedestal_frame(img)
    cf_frz.push_pedestal_frame(img)

f.seek(0)
data = f.read_n(TARGET_FRAME + 1)

# state entering the target frame
for fid in range(TARGET_FRAME):
    cf_cpu.find_clusters(data[fid]); cf_cpu.steal_clusters(realloc_same_capacity=True)
    cf_frz.find_clusters(data[fid]); cf_frz.steal_clusters(realloc_same_capacity=True)

ped_cpu_before = np.asarray(cf_cpu.pedestal).copy()
ped_frz_before = np.asarray(cf_frz.pedestal).copy()
rms_cpu_before = np.asarray(cf_cpu.noise).copy()
rms_frz_before = np.asarray(cf_frz.noise).copy()

# the frame itself
cf_cpu.find_clusters(data[TARGET_FRAME])
cf_frz.find_clusters(data[TARGET_FRAME])
b_cpu = np.asarray(cf_cpu.branch_map)
b_frz = np.asarray(cf_frz.branch_map)

sl = (slice(TY - 1, TY + 2), slice(TX - 1, TX + 2))
raw = data[TARGET_FRAME][sl].astype(float)

# The frozen model reads its frame-start snapshot for the whole frame, so
# ped_frz_before IS what it used. The serial model's 3-above/1-left neighbours
# may already carry this frame's sample by the time (TY,TX) is tested, so its
# effective pedestal is read AFTER the frame -- but only for pixels it pushed.
ped_cpu_after = np.asarray(cf_cpu.pedestal).copy()

# raster order: the four already-scanned neighbours of (TY,TX)
scanned = np.zeros((3, 3), dtype=bool)
scanned[0, :] = True          # the row above: (-1,-1) (-1,0) (-1,+1)
scanned[1, 0] = True          # and the pixel to the left

ped_cpu_used = np.where(scanned, ped_cpu_after[sl], ped_cpu_before[sl])
ped_frz_used = ped_frz_before[sl]

val_cpu = raw - ped_cpu_used
val_frz = raw - ped_frz_used
rms = float(rms_frz_before[TY, TX])

rec = dict(
    frame=TARGET_FRAME, iy=TY, ix=TX, n_sigma=N_SIGMA, c3=float(C3),
    raw=raw.tolist(),
    ped_cpu=ped_cpu_used.tolist(), ped_frz=ped_frz_used.tolist(),
    val_cpu=val_cpu.tolist(), val_frz=val_frz.tolist(),
    scanned=scanned.tolist(),
    rms_cpu=float(rms_cpu_before[TY, TX]), rms_frz=rms,
    total_cpu=float(val_cpu.sum()), total_frz=float(val_frz.sum()),
    max_cpu=float(val_cpu.max()), max_frz=float(val_frz.max()),
    value_cpu=float(val_cpu[1, 1]), value_frz=float(val_frz[1, 1]),
    thr_test1_cpu=float(N_SIGMA * rms_cpu_before[TY, TX]),
    thr_test1_frz=float(N_SIGMA * rms),
    thr_test3_cpu=float(C3 * N_SIGMA * rms_cpu_before[TY, TX]),
    thr_test3_frz=float(C3 * N_SIGMA * rms),
    branch_cpu=int(b_cpu[TY, TX]), branch_frz=int(b_frz[TY, TX]),
)
# A wider patch for the context panel: what the raster was doing around the
# site. Branch codes come from the SERIAL finder, since the left panel's job is
# to show the scan in progress.
R = 10
py0, px0 = TY - R, TX - R
pat = (slice(py0, TY + R + 1), slice(px0, TX + R + 1))
rec["patch"] = dict(
    r=R, y0=int(py0), x0=int(px0),
    val=(data[TARGET_FRAME][pat].astype(float) - ped_frz_before[pat]).tolist(),
    branch_cpu=b_cpu[pat].astype(int).tolist(),
    branch_frz=b_frz[pat].astype(int).tolist(),
)

json.dump(rec, open(OUT / 'branch_site.json', 'w'), indent=1)

print(f"frame {TARGET_FRAME}, pixel ({TY},{TX})   branch cpu={rec['branch_cpu']} "
      f"frozen={rec['branch_frz']}   (4=QUIET_UPDATE, 3=TEST3_STORE)")
print(f"  rms {rms:.3f}   Test1 thr {rec['thr_test1_frz']:.2f}   "
      f"Test3 thr {rec['thr_test3_frz']:.2f}")
print(f"  serial : max {rec['max_cpu']:8.3f}  total {rec['total_cpu']:8.3f}  "
      f"-> Test3 {'PASS' if rec['total_cpu'] > rec['thr_test3_cpu'] else 'fail'}")
print(f"  frozen : max {rec['max_frz']:8.3f}  total {rec['total_frz']:8.3f}  "
      f"-> Test3 {'PASS' if rec['total_frz'] > rec['thr_test3_frz'] else 'fail'}")
print(f"  the four already-scanned neighbours differ by "
      f"{np.abs(ped_cpu_used - ped_frz_used)[scanned].sum():.4f} ADU total")
print(f"wrote {OUT / 'branch_site.json'}")
