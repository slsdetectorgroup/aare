# Minimal probe for nsys: train pedestal, run one batched pass, print summary.
# Usage: nsys_kernel_probe.py [n_streams] [n_frames]
import sys
sys.path.append('/home/ferjao_k/aare/build')

from pathlib import Path
import time
from aare import File, ClusterFinderCUDA

n_streams = int(sys.argv[1]) if len(sys.argv) > 1 else 8
N         = int(sys.argv[2]) if len(sys.argv) > 2 else 2000

base = Path('/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/')
f  = File(base / 'Cu_factor_10_data_master_0.json')
pd = File(base / 'Cu_factor_10_pedestal_master_0.json')

cf = ClusterFinderCUDA((f.rows, f.cols), (3, 3), n_sigma=5,
                       max_clusters_per_frame=3000, n_streams=n_streams)
for _ in range(1000):
    cf.push_pedestal_frame(pd.read_frame().copy())

data = f.read_n(N)
cf.register_input_buffer(data)

t0 = time.perf_counter()
res = cf.find_clusters_batched(data, first_frame=0)
t = time.perf_counter() - t0

cf.unregister_input_buffer()
n = sum(cv.size for cv in res)
print(f'n_streams={n_streams}  N={N}  wall={t:.3f}s  ({N/t:.0f} FPS)  '
      f'clusters/frame={n/N:.2f}  event kernel_ms={cf.avg_kernel_time_ms():.3f}')