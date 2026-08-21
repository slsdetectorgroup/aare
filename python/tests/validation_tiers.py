"""Tiered CPU/CUDA agreement study for the deck's validation slides.

Runs the three finders that differ in exactly one thing each over the same
frames, from the same pedestal, and scores every pair:

    ClusterFinder        serial CPU, pedestal pushed DURING the raster scan
    ClusterFinderFrozen  same logic, pedestal frozen per frame + deferred push
    ClusterFinderCUDA    frozen per frame, float32 device pedestal

so that   serial vs frozen = update timing alone (a CPU-only effect)
and       frozen vs cuda   = everything CUDA changes.

The question the deck needs answered is not "how many disagree" but "in which
direction, and is a CUDA-only centre an invented photon or a second copy of one
both finders already found". So every CUDA-only centre is also scored at tol=1:
if it has a counterpart in the agreed set's 8-neighbourhood it is a duplicate,
not an invention.

Writes tiers.json + spectra_valid.png next to itself.
"""
import sys, json, time
sys.path.append('/home/ferjao_k/aare/build')
sys.path.append('/home/ferjao_k/aare/python/tests')

from pathlib import Path
import numpy as np
import boost_histogram as bh
import matplotlib
matplotlib.use("Agg")

from aare import File, ClusterFinder, ClusterFinderFrozen, ClusterFinderCUDA
from helper import centers, only_sets, shift_dist

OUT = Path(__file__).resolve().parent
BASE = Path('/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/'
            'process/xrf/')

N_PED, N, N_SIGMA, N_STREAMS = 1000, 10000, 5, 4
CLUSTER = (3, 3)
IMG = (400, 400)
CAP = 50_000
NBINS, ERANGE = 200, (-2, 4000)

f = File(BASE / 'Cu_factor_10_data_master_0.json')
pd = File(BASE / 'Cu_factor_10_pedestal_master_0.json')

cf_cpu = ClusterFinder(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)
cf_frz = ClusterFinderFrozen(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)
cf_cud = ClusterFinderCUDA(IMG, CLUSTER, n_sigma=N_SIGMA,
                           max_clusters_per_frame=3000, n_streams=N_STREAMS)
finders = {'cpu': cf_cpu, 'frozen': cf_frz, 'cuda': cf_cud}

t0 = time.perf_counter()
pd.seek(0)
for _ in range(N_PED):
    img = pd.read_frame().copy()
    for cf in finders.values():
        cf.push_pedestal_frame(img)
print(f'pedestal train: {time.perf_counter()-t0:.1f}s', flush=True)

f.seek(0)
data = f.read_n(N)
print('data:', data.shape, data.dtype, flush=True)

names = list(finders)
totals = {n: 0 for n in names}
hists = {n: bh.Histogram(bh.axis.Regular(NBINS, *ERANGE)) for n in names}
pairs = {(a, b): dict(a_only=0, b_only=0) for i, a in enumerate(names)
         for b in names[i + 1:]}

# every CUDA-only centre, scored against the agreed set
extras = []          # one record per frozen-vs-cuda cuda-only centre
n_dup_tol1 = 0

t0 = time.perf_counter()
for fid in range(N):
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
        acc['a_only'] += len(a_only)
        acc['b_only'] += len(b_only)

    # the tier that matters: frozen vs cuda, one record per extra
    _, cu_only = only_sets(cs['frozen'], cs['cuda'], tol=0)
    for p in cu_only:
        d = shift_dist(p, cs['frozen'], R=4)
        extras.append(dict(frame=int(fid), x=int(p[0]), y=int(p[1]),
                           shift=int(d)))
    if cu_only:
        _, cu_only_1 = only_sets(cs['frozen'], cs['cuda'], tol=1)
        n_dup_tol1 += len(cu_only) - len(cu_only_1)

    if fid % 1000 == 0:
        print(f'  {fid}/{N}  {time.perf_counter()-t0:.0f}s', flush=True)

print(f'scan: {time.perf_counter()-t0:.0f}s', flush=True)

res = dict(n_frames=N, totals=totals,
           pairs={f'{a} vs {b}': v for (a, b), v in pairs.items()},
           extras=extras,
           n_cuda_only=len(extras),
           n_adjacent_to_agreed=n_dup_tol1,
           shift_histogram={str(k): int(v) for k, v in
                            zip(*np.unique([e['shift'] for e in extras],
                                           return_counts=True))} if extras else {},
           hists={n: h.values().tolist() for n, h in hists.items()},
           edges=hists[names[0]].axes[0].edges.tolist())
(OUT / 'tiers.json').write_text(json.dumps(res))

print('\n=== totals ===')
for n in names:
    print(f'  {n:8s} {totals[n]:>12,}')
print('\n=== pairwise (tol=0) ===')
for (a, b), v in pairs.items():
    tot = v['a_only'] + v['b_only']
    print(f'  {a:>6s} vs {b:<6s}  {a}-only {v["a_only"]:>4}   '
          f'{b}-only {v["b_only"]:>4}   total {tot:>4}  '
          f'({tot/max(totals[a],1):.2e})')
print('\n=== the frozen-vs-cuda extras ===')
print(f'  cuda-only centres (tol=0):        {len(extras)}')
print(f'  of which adjacent to an agreed centre (tol=1): {n_dup_tol1}')
print(f'  chebyshev shift to nearest frozen centre: {res["shift_histogram"]}')
