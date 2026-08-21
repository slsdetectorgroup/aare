# INVALID — do not cite these numbers

Probes run 2026-08-20 for 9x9 at cap 1700, discarded.

`env.json` in this directory says `"device_ped_type": "float"`. **It is wrong.**
`common.device_ped_type()` parses `include/aare/clusterfinder_kernel.cuh` — the
*source* — and the tree had not been rebuilt after that header was edited:

    header edited : 2026-08-20 10:54:29
    binary built  : 2026-08-20 09:57:53      <- 57 minutes EARLIER

So the header read `float/float` while the loaded module was still the
`COMPUTE_TYPE = double; DEVICE_PED_TYPE = double` build used for the 9x9
validation study. The measurement is a full-f64 kernel labelled f32.

The giveaway is the kernel time, which the cap cannot affect:

    9x9 s4   kernel 80.73 us    (the genuine f32 build reads 24.25)
    9x9 s1   kernel 87.92 us    (the genuine f32 build reads 23.67)

Re-run after `make install`, and only once the guard in common.py confirms the
binary is newer than the header.

One number here is still worth reading, because the cluster payload type is
`int32` regardless of COMPUTE_TYPE, so the D2H byte count is build-independent:

    cap 1500 -> 492 004 B/frame -> D2H 22.84 us   (2026-08-18_f32)
    cap 1700 -> 557 600 B/frame -> D2H 22.70 us   (here)

13.3 % more bytes, no more time. D2H is not bandwidth-bound at this size, which
contradicts the linear extrapolation used to predict "cap 1700 makes D2H
overtake the kernel". To be confirmed on a clean build.
