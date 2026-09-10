"""The process nsys traces: train the pedestal, run one batched pass, exit.

    nsys_kernel_probe.py <n_streams> <n_frames> <cluster_dim> <cap> <batch>

Run by run_probes.py; run directly only under `nsys profile --trace=cuda`.
The wall time printed is profiler-inflated and is not a throughput number.
"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402  (puts the build dir on sys.path)

from aare import File, ClusterFinderCUDA  # noqa: E402

n_streams, N, cdim, cap, batch = (int(a) for a in sys.argv[1:6])

f = File(common.DATA_FILE)
pd = File(common.PEDESTAL_FILE)

cf = ClusterFinderCUDA((f.rows, f.cols), (cdim, cdim), n_sigma=common.N_SIGMA,
                       max_clusters_per_frame=cap, n_streams=n_streams)
for _ in range(common.N_PEDESTAL_FRAMES):
    cf.push_pedestal_frame(pd.read_frame().copy())

data = f.read_n(N)
cf.register_input_buffer(data)
# Pin the output slots without processing frames: a warm-up pass would advance
# the device pedestal and make this run incomparable with the next.
cf.reserve_output_slots(cf.chunk_size_for(min(N, batch)))

t0 = time.perf_counter()
n = 0
for start in range(0, N, batch):
    for cv in cf.find_clusters_batched(data[start:min(start + batch, N)],
                                       first_frame=start):
        n += cv.size
t = time.perf_counter() - t0
cf.unregister_input_buffer()

h2d = f.rows * f.cols * 2
slot = 2 + 2 + cdim * cdim * 4  # x, y (uint16) + data (int32)
print(f"n_streams={n_streams}  N={N}  cluster={cdim}x{cdim}  cap={cap}  batch={batch}")
print(f"  H2D/frame={h2d:,} B   D2H/frame={cap * slot:,} B "
      f"({slot} B/slot, {100 * n / N / cap:.0f}% filled)")
print(f"  wall={t:.3f}s  ({N / t:.0f} FPS, profiler-inflated)  clusters/frame={n / N:.2f}")
