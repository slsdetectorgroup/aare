# Minimal probe for nsys: train pedestal, run one batched pass, print summary.
# Usage: nsys_kernel_probe.py [n_streams] [n_frames] [cluster_dim] [cap] [batch]
#
#   nsys profile --trace=cuda --sample=none --cpuctxsw=none -o rep \
#        python nsys_kernel_probe.py 4 2000 9 1700
#   nsys stats --report cuda_gpu_sum rep.nsys-rep     # per-op totals
#   python gpu_span.py rep.sqlite 2000                # engine duty cycles
#
# The wall time printed here is NOT a throughput number: a fresh process pays the
# full first-touch page-fault tax inside the timed call, and nsys inflates it
# further. Take per-operation GPU times from the reports, wall times from the
# notebook. See docs/ClusterFinderCUDA_benchmark_results.md sections 3.3 and 14.
import sys
sys.path.append('/home/ferjao_k/aare/build')

from pathlib import Path
import time
from aare import File, ClusterFinderCUDA

n_streams = int(sys.argv[1]) if len(sys.argv) > 1 else 8
N         = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
cdim      = int(sys.argv[3]) if len(sys.argv) > 3 else 9
cap       = int(sys.argv[4]) if len(sys.argv) > 4 else 1700
# Loop in BATCH_SIZE slices exactly as run_ladder.py does, so the duty cycles
# describe the configuration the throughput numbers were taken on. One giant
# call would chunk differently and give a different overlap picture.
batch     = int(sys.argv[5]) if len(sys.argv) > 5 else 2000

base = Path('/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/')
f  = File(base / 'Cu_factor_10_data_master_0.json')
pd = File(base / 'Cu_factor_10_pedestal_master_0.json')

cf = ClusterFinderCUDA((f.rows, f.cols), (cdim, cdim), n_sigma=5,
                       max_clusters_per_frame=cap, n_streams=n_streams)
for _ in range(1000):
    cf.push_pedestal_frame(pd.read_frame().copy())

data = f.read_n(N)
cf.register_input_buffer(data)

cf.reserve_output_slots(cf.chunk_size_for(min(N, batch)))

t0 = time.perf_counter()
n = 0
for start in range(0, N, batch):
    stop = min(start + batch, N)
    # Counted and discarded per batch, matching run_ladder.py's default
    # consumer: peak result memory is one batch, not the whole run.
    for cv in cf.find_clusters_batched(data[start:stop], first_frame=start):
        n += cv.size
t = time.perf_counter() - t0

cf.unregister_input_buffer()

# Transfer sizes per frame, so the nsys memcpy rows can be checked against the
# payload they carry: the D2H is cap-sized regardless of how many clusters were
# found, which is what makes `cap` a throughput knob and not just a safety bound.
h2d = f.rows * f.cols * 2
slot = 2 + 2 + cdim * cdim * 4                 # x, y (uint16) + data (int32)
print(f'n_streams={n_streams}  N={N}  cluster={cdim}x{cdim}  cap={cap}  batch={batch}')
print(f'  H2D/frame={h2d:,} B   D2H/frame={cap * slot:,} B '
      f'({slot} B/slot, {100 * n / N / cap:.0f}% filled)')
print(f'  wall={t:.3f}s  ({N/t:.0f} FPS, profiler-inflated)  clusters/frame={n/N:.2f}')
if cf.kernel_timing_enabled():
    print(f'  event kernel_ms={cf.avg_kernel_time_ms():.3f}  '
          f'(only meaningful at n_streams=1)')