# 9×9 cap A/B — the `[f64]` arm

**Build**: `COMPUTE_TYPE = float`, `DEVICE_PED_TYPE = double`, git `7177f00` — the
mixed configuration the campaign's "f64 arm" has always been, not double/double.
**Config**: 9×9, 20 000 frames, batch 2000, idle GPU, `assert_build_fresh()` passed.

Companion to `../2026-08-20_f32_capAB/README.md`, which explains the mechanism
(the D2H slot is `4 + cap × 328 B` at 9×9 and is copied whole every frame) and
why cap 1500 was truncating. Read that one first.

## The measurement

| cap | streams | kernel | H2D | D2H | binds | roofline |
|--:|--:|--:|--:|--:|---|--:|
| 1500 | 4 | 31.86 | 19.90 | 22.44 | kernel | 31.86 µs → 31 392 |
| 1500 | 1 | 39.78 | 13.18 | 19.48 | kernel | 39.78 µs → 25 139 |
| 1700 | 4 | 32.66 | 20.77 | 25.25 | **kernel** | 32.66 µs → 30 621 |
| 1700 | 1 | 39.86 | 13.20 | 21.97 | kernel | 39.86 µs → 25 086 |

Sustained (`../2026-08-20_f64_cap1700/ladder_9x9.csv`): opt8 reaches
**33 323 FPS / 30.01 µs** at cap 1700, against 33 301 / 30.03 at cap 1500.

## Why the cap is free here and not on `[f32]`

D2H grows identically on both arms — 22.4 → 25.2 µs — because the cluster payload
is `int32` regardless of `COMPUTE_TYPE`, so the slot is the same 328 B either way.
What differs is what it has to climb over:

    arm    kernel @ s4    D2H @ cap 1700    binds        opt8 cost of the cap
    f64        32.66           25.25        kernel       +0.1 %  (nothing)
    f32        23.94           25.24        D2H          -5.9 %

On the f64 arm the kernel is tall enough to hide any cap worth setting: 25.25 µs
of D2H disappears entirely underneath a 32.66 µs kernel, and opt8 lands on the
same 30.0 µs it did at cap 1500.

**opt7 is what makes the cap expensive.** Dropping the kernel 40 % — 32.66 → 23.94
µs — moves it *below* the enlarged D2H bar. The result path was never the
constraint until the kernel stopped being one. That is the same rule the whole
ladder is ordered by, appearing once more and at the last possible moment: you
cannot see the result path until the kernel gets out of its way.

So the honest statement about Act III is not that the cap invalidates it. It is:

- the kernel optimization is worth its full −40 % at cap 1500 (kernel-bound), and
- at a lossless cap it is worth −40 % of a bar that is no longer the tallest,
  which is what success looks like when you optimize in bottleneck order.

## Non-obvious: opt5 loses 5.2 %, opt3/opt4 lose nothing

    opt3   12 170 -> 12 129   -0.3 %      run noise
    opt4   12 431 -> 12 527   +0.8 %      run noise
    opt5   15 883 -> 15 063   -5.2 %      real
    opt8   33 301 -> 33 323   +0.1 %      run noise

opt3 and opt4 sit at ~30 % of the floor: they are host-bound with the GPU idle
most of the frame, so 65 kB more per frame vanishes into slack. opt8 sits *on*
the floor, but on this arm the floor is the kernel, which did not move. opt5 is
the one caught in between — overlapped enough that transfer time is on the
critical path, not fast enough to be floor-bound — so the extra bytes bill in
full. The cap's cost is not uniform across the ladder; it depends on what each
step is limited by.

## Artifacts

`probe_9x9_{s4,s1_uncontended}_cap{1500,1700}.nsys-rep` / `.sqlite`, and
`probes.csv` carrying `cap` as a column. Filenames include the cap because two
probes of one label at two caps are different measurements.
