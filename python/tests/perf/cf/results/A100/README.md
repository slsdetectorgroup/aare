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

`lround(float)` is emulated in software on the device and made the f32 9x9
kernel 1.8x slower than f64. It was replaced by `round` in 104e284. The
2026-10-28 runs used local, uncommitted edits to `clusterfinder_kernel.cuh`
(the precision aliases), so `env.json` shows `git_dirty: true` and does not
record the rounding.

Event trace on and off give the same kernel times; with it on, the idle gaps
between steps are 10-16 % wider.

