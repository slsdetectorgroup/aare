# A100 probe results (Merlin7)

NVIDIA A100 on Merlin7 (gmerlin7, a100 partitions), CUDA 12.9.1, gcc 13.3.0,
nsys 2025.1.3. CUDA event trace is off (`--cuda-event-trace=false`) unless the
folder name says `evtrace-on`. Driver and git state of each run are in its
`env.json`.

| folder | precision | copy-out rounding | frames / batch | configs |
|---|---|---|---|---|
| `2026-10-28_f32_lround_evtrace-on` | f32 | `lround` | 20 000 / 2000 | 3x3, 9x9 at s1, s4 |
| `2026-10-28_f32_lround` | f32 | `lround` | 20 000 / 2000 | 3x3, 9x9 at s1, s4 |
| `2026-10-28_f64_lround` | f64 | `lround` | 20 000 / 2000 | 3x3, 9x9 at s1, s4 |
| `2026-10-29_f32_round_b2000_f20K` | f32 | `round` | 20 000 / 2000 | 3x3 to 9x9 at s1, s4, s8 |
| `2026-10-29_f32_round_b3000_f50K` | f32 | `round` | 50 000 / 3000 | 3x3 to 9x9 at s1, s4, s8 |
| `2026-10-29_f32_round_throughput` | f32 | `round` | 20 000 / 2000 | unprofiled wall clock (`throughput.py`), 3x3 to 9x9 at s1, s4 |
| `2026-10-29_f32_round_throughput_5x5` | f32 | `round` | 20 000 / 2000 | 5x5 rerun, 5 passes (first run disturbed by other jobs on the node) |

## Result: the copies limit every configuration

With `round`, the kernel is never the slowest engine: in all 12
configurations an H2D or D2H copy takes longer. Busy time per frame (µs),
`2026-10-29_f32_round_b3000_f50K`; the 20 000-frame run agrees to within 5 %
on the kernel and gives the same limiting engine everywhere.

| cluster | streams | kernel | H2D | D2H | limited by |
|---|--:|--:|--:|--:|---|
| 3x3 | 1 | 6.5 | 13.9 | 5.9 | H2D |
| 3x3 | 4 | 7.6 | 17.5 | 8.2 | H2D |
| 3x3 | 8 | 8.2 | 17.6 | 8.3 | H2D |
| 5x5 | 1 | 8.4 | 14.0 | 10.7 | H2D |
| 5x5 | 4 | 9.2 | 20.4 | 13.4 | H2D |
| 5x5 | 8 | 11.1 | 20.8 | 14.0 | H2D |
| 7x7 | 1 | 10.8 | 14.2 | 18.8 | D2H |
| 7x7 | 4 | 11.5 | 23.9 | 22.5 | H2D |
| 7x7 | 8 | 12.5 | 25.4 | 23.0 | H2D |
| 9x9 | 1 | 14.5 | 14.2 | 24.4 | D2H |
| 9x9 | 4 | 15.2 | 26.6 | 28.2 | D2H |
| 9x9 | 8 | 16.0 | 26.8 | 28.3 | D2H |

Copies get slower per frame with more streams because H2D and D2H then share
the PCIe link (see "Reading the numbers" in [`../../README.md`](../../README.md)).

## `lround` vs `round`

`lround(float)` is emulated in software on the device. With it, the f32 9x9
kernel was 1.83x slower than f64 at one stream and 1.69x at four (3x3: 1.01x
and 0.93x). It was replaced by `round` in 104e284.

The 2026-10-28 runs record `git_rev` `b4d302d`, a commit from before a rebase
of `feature/cuda_pixelhistogram`. Its cluster-finder code is that of
`b7b786f` on `feature/cuda_clusterfinder`, the commit before 104e284. They
also used local, uncommitted edits to `clusterfinder_kernel.cuh` (the
precision aliases), so `env.json` does not record the precision or the
rounding: the folder names do. `git_dirty` is `true` in every `env.json` here
because the flag counted untracked files at the time; only the 2026-10-28 runs
had local edits.

## Event trace

Event trace on (`2026-10-28_f32_lround_evtrace-on`, node gpu105) and off
(`2026-10-28_f32_lround`, node gpu102) give the same kernel and copy times, to
within 1 % (H2D 4 % at 3x3, four streams). The time per frame under nsys is
longer with event trace on by 11 % (3x3) and 19 % (9x9) at four streams and by
under 2 % at one stream. The two runs were on different nodes, and four-stream
runs vary by up to 20 % between sessions, so this does not isolate the cost of
event tracing.
