"""Which test is responsible for the serial-vs-frozen disagreement?

ClusterFinder and ClusterFinderFrozen differ in exactly one thing: WHEN the
pedestal is pushed. `push_fast` touches only the pixel's own accumulators and
never reads the stencil, so the update itself is order-independent: given the
same set of updated pixels, both models end a frame with a bit-identical
pedestal. The only channel for divergence is therefore a differing DECISION.

There are three places a decision can differ, and they are not equally exposed:

  Test1   max > nSigma*rms       reads the stencil MAX.
  local-max gate  v == m         compares two stencil values.
  Test3   total > c3*nSigma*rms  reads the stencil SUM, so it collects the
                                 shift of every already-scanned neighbour --
                                 three above and one left -- where Test1 feels
                                 at most the one that happens to be the argmax.

WHAT THIS ACTUALLY FOUND (the prior stated here before running it was only half
right, so it is recorded as measured rather than as predicted):

  * Every cluster frozen finds and serial does not -- 11 of them -- comes from
    Test3. Compiling Test3 out sends that count to exactly zero.
  * Every cluster serial finds and frozen does not -- 8 -- comes from the
    local-max gate, downstream of accumulated pedestal drift.
  * Test1's threshold flips MOST often, 619 of 974 divergent pixels, and yields
    ZERO clusters: it only moves a pixel between QUIET_UPDATE and SHADOW, and
    neither of those stores.
  * Test1 is not merely downstream of Test3. With Test3 ablated the first
    divergence is still a Test1 flip, on a frame where the two pedestals were
    provably identical. It initiates on its own, just ~7x slower (frame 30
    against frame 4), and cannot create a cluster by itself.

This does not ablate anything -- both finders run their shipped logic. Each
records a per-pixel branch code (see branch_map in either header) and we diff
the two maps frame by frame, which names the guilty test on the real algorithm
and localises it for the annex figure.

Writes branch_trace.json next to itself.

REQUIRES a build with branch tracing on, which is OFF by default so the
shipped path carries no per-pixel store:

    sed -i 's/#define AARE_BRANCH_TRACE 0/#define AARE_BRANCH_TRACE 1/' \
        include/aare/ClusterFinder.hpp include/aare/ClusterFinderFrozen.hpp
    cmake --build build -j8

Set it back to 0 afterwards. Without it branch_map is all-5 and this
script reports no divergence at all.
"""
import sys, json, time
sys.path.append('/home/ferjao_k/aare/build')
sys.path.append('/home/ferjao_k/aare/python/tests')

from pathlib import Path
from collections import Counter
import numpy as np

from aare import File, ClusterFinder, ClusterFinderFrozen
from helper import centers, only_sets

OUT = Path(__file__).resolve().parent
BASE = Path('/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/'
            'process/xrf/')

N_PED, N, N_SIGMA = 1000, 10000, 5
CLUSTER = (3, 3)
IMG = (400, 400)
CAP = 50_000

CODE = {0: "NEG", 1: "SHADOW", 2: "TEST1_STORE", 3: "TEST3_STORE",
        6: "TEST3_SKIP", 4: "QUIET_UPDATE", 5: "UNTOUCHED"}

f = File(BASE / 'Cu_factor_10_data_master_0.json')
pd = File(BASE / 'Cu_factor_10_pedestal_master_0.json')

cf_cpu = ClusterFinder(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)
cf_frz = ClusterFinderFrozen(IMG, CLUSTER, n_sigma=N_SIGMA, capacity=CAP)

t0 = time.perf_counter()
pd.seek(0)
for _ in range(N_PED):
    img = pd.read_frame().copy()
    cf_cpu.push_pedestal_frame(img)
    cf_frz.push_pedestal_frame(img)
print(f'pedestal train: {time.perf_counter()-t0:.1f}s', flush=True)

f.seek(0)
data = f.read_n(N)
print('data:', data.shape, data.dtype, flush=True)

# (cpu_code, frozen_code) -> count, over every pixel where the two maps differ
transitions = Counter()
# the first frame at which the branch maps diverge at all
first_div = None
# per-frame centre-set difference, to reproduce the 8/11 headline
cpu_only_tot = frz_only_tot = 0
n_cpu = n_frz = 0
# every divergent pixel, kept for the figure (there should be very few)
sites = []

t0 = time.perf_counter()
for fid in range(N):
    cf_cpu.find_clusters(data[fid])
    cf_frz.find_clusters(data[fid])
    b_cpu = np.asarray(cf_cpu.branch_map)
    b_frz = np.asarray(cf_frz.branch_map)

    cv_cpu = cf_cpu.steal_clusters(realloc_same_capacity=True)
    cv_frz = cf_frz.steal_clusters(realloc_same_capacity=True)
    c_cpu, c_frz = centers(cv_cpu), centers(cv_frz)
    n_cpu += len(c_cpu); n_frz += len(c_frz)
    a_only, b_only = only_sets(c_cpu, c_frz, tol=0)
    cpu_only_tot += len(a_only); frz_only_tot += len(b_only)

    diff = np.argwhere(b_cpu != b_frz)
    if diff.size:
        if first_div is None:
            first_div = int(fid)
        for iy, ix in diff:
            pair = (int(b_cpu[iy, ix]), int(b_frz[iy, ix]))
            transitions[pair] += 1
            if len(sites) < 400:
                sites.append(dict(frame=int(fid), iy=int(iy), ix=int(ix),
                                  cpu=CODE[pair[0]], frozen=CODE[pair[1]]))
    if (fid + 1) % 2000 == 0:
        print(f'  {fid+1}/{N}  divergent pixels so far: '
              f'{sum(transitions.values())}', flush=True)

dt = time.perf_counter() - t0
print(f'\nscan: {dt:.1f}s')
print(f'clusters: cpu {n_cpu}  frozen {n_frz}')
print(f'centre-set difference (the 8/11 headline): '
      f'cpu-only {cpu_only_tot}  frozen-only {frz_only_tot}')
print(f'first frame with any branch divergence: {first_div}')
print(f'\ndivergent pixels, by (cpu branch -> frozen branch):')
for (a, b), n in transitions.most_common():
    print(f'  {CODE[a]:>12}  ->  {CODE[b]:<12}  {n:>8}')

json.dump(dict(n_frames=N, n_ped=N_PED, n_sigma=N_SIGMA,
               clusters=dict(cpu=n_cpu, frozen=n_frz),
               centre_diff=dict(cpu_only=cpu_only_tot,
                                frozen_only=frz_only_tot),
               first_divergent_frame=first_div,
               transitions={f'{CODE[a]}->{CODE[b]}': n
                            for (a, b), n in transitions.items()},
               sites=sites),
          open(OUT / 'branch_trace.json', 'w'), indent=1)
print(f'\nwrote {OUT / "branch_trace.json"}')
