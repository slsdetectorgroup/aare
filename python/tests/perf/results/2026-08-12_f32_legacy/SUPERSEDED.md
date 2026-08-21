# Superseded — do not cite

These profiles predate the `perf/` harness. They are kept only because they are
the provenance for numbers still quoted in older revisions of
`docs/ClusterFinderCUDA_benchmark_results.md`. **Every one of them has been replaced** by
`../2026-08-18_f32/` and `../2026-08-18_f64/`.

Three reasons they were retired rather than reused:

1. **2 000 frames.** Too short for the GPU clocks to ramp (210 MHz idle ->
   3.1 GHz boost), which under-reports the GPU by ~7-10 %. This is how a
   26.7 us/frame roofline was published for a pipeline that sustains 24.25.
2. **No build record.** Nothing in these files says which `DEVICE_PED_TYPE` they
   were taken on. `probe_s1.nsys-rep` was described in the report as the f64
   reference; `nsys stats` shows a 25.2 us kernel, i.e. it is **f32**. The
   entire f64 arm was therefore unsupported until the 2026-08-18 campaign.
3. **One giant call, not the ladder's batching.** They drove
   `find_clusters_batched(all_frames)` rather than looping in BATCH_SIZE
   slices, so their overlap and duty cycles describe a configuration the
   throughput numbers were never taken on.

| file | what it actually is |
|---|---|
| `probe_s1.nsys-rep` | 9x9, 1 stream, **f32** (25.2 us kernel) — mislabelled as f64 in the report |
| `probe3x3_s4.*` | 3x3, 4 streams, events ON |
| `probe3x3_s4_no_timing.*` | 3x3, 4 streams, events OFF |
| `probe3x3_s1_no_timing.*` | 3x3, 1 stream |
| `probe9x9_s1_no_timing_cap_1500.*` | 9x9, 1 stream, cap 1500 |
| `probe9x9_s1_no_timing.*` | 9x9, 1 stream, cap 3000 |
| `probe9x9_s4_no_timing.*` | 9x9, 4 streams, cap 3000 |

Safe to delete once the report cites only the 2026-08-18 campaign.
