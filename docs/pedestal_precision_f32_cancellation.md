# CUDA ClusterFinder — why an f32 pedestal breaks (catastrophic cancellation)

## TL;DR

The CUDA kernel computes the per-pixel pedestal variance as
`var = E[X²] − E[X]²` in `DEVICE_PED_TYPE`. Pedestals sit at **~4655 ADU**, so it
recovers a **~2025** variance by subtracting two **~2.17×10⁷** numbers. In
**float32** (~7 significant digits) that subtraction carries a fixed **±3 ADU²**
absolute error: negligible for noisy pixels, but it drives low-noise pixels'
`rms²` to zero (via the clamp on the next line), turning ~1–2 % of pixels into
hot pixels that fire every frame → **+28 % spurious clusters and an unphysical
high-energy tail**. **float64** has the digits to spare, so it never happens
(CPU↔CUDA agree to 0.0001 %).

**Fix:** either ship `DEVICE_PED_TYPE = double`, or keep f32 and remove the
cancellation (per-pixel offset accumulation, or Welford online variance).

---

## 1. What was observed

Two builds of `aare`, same data (MOENCH 400×400, Cu XRF, 1000 pedestal frames,
20k–50k data frames, `n_sigma = 5`):

| build | `COMPUTE_TYPE` | `DEVICE_PED_TYPE` | CPU↔CUDA agreement |
|---|---|---|---|
| correctness | double (f64) | **double (f64)** | ~100 % (35 / 46,478,563 clusters = **0.0001 %**) |
| performance | double (f64) | **float (f32)** | CPU 116,010,113 vs CUDA 148,559,598 = **+28.06 %** |

The f32-pedestal build also shows a large **unphysical tail** in the
cluster-energy spectrum. The *compute* precision (f64 in both) is not the
variable — the **pedestal** precision is.

---

## 2. What the kernel computes

Per pixel, in `include/aare/clusterfinder_kernel.cuh` (~lines 186–191):

```cpp
DEVICE_PED_TYPE var_px =
    d_pd_sum2[global_tid] / n  -  mean_px * mean_px;      // variance = E[X²] − E[X]²
COMPUTE_TYPE rms_sq = (var_px > 0) ? var_px : 0;          // clamp negatives to 0
COMPUTE_TYPE nSig_sq_rms_sq = nSigma * nSigma * rms_sq;   // squared threshold
```

Decision (Test 1): a pixel is a photon if `max_val² > nSig_sq_rms_sq`, i.e.
`max_val > nSigma · rms`. Everything hinges on `rms_sq` being right.

The stored pedestal accumulators (in `DEVICE_PED_TYPE`), at equilibrium:

- `d_pd_sum[tid]  ≈ n · mean`
- `d_pd_sum2[tid] ≈ n · E[X²]`

---

## 3. The magnitudes (with `mean = 4655`, `rms = 45`, `n = 1000`)

| quantity | formula | value |
|---|---|---|
| `mean` | — | 4 655 |
| `mean²` | 4655² | **21 669 025** ≈ 2.17×10⁷ |
| `E[X²]` | `mean² + var` = 21 669 025 + 2025 | **21 671 050** ≈ 2.17×10⁷ |
| `var` | `E[X²] − mean²` | **2 025** |
| `sum2` | `n · E[X²]` = 1000 × 21 671 050 | **2.167×10¹⁰** |

Critical ratio:

```
var / mean² = 2025 / 21 669 025 ≈ 9×10⁻⁵
```

**We recover a number (2025) ~11 000× smaller than the two numbers subtracted to
get it.** That is the entire problem.

---

## 4. float32 precision

IEEE-754 single = 24 significant bits:

- **≈ 7.2 significant decimal digits** (24 · log₁₀2).
- **ULP rule:** a value in `[2ᵉ, 2ᵉ⁺¹)` lies on a grid of spacing `ULP = 2ᵉ⁻²³`.
  Absolute precision therefore *grows* with magnitude.

Applied to each quantity (find `e` with `2ᵉ ≤ value < 2ᵉ⁺¹`):

| quantity | value | range | e | ULP = 2ᵉ⁻²³ |
|---|---|---|---|---|
| `mean` | 4 655 | [2¹², 2¹³) | 12 | 2⁻¹¹ ≈ **0.0005** |
| `mean²` | 2.17×10⁷ | [2²⁴, 2²⁵) | 24 | 2¹ = **2** |
| `E[X²]` = `sum2/n` | 2.17×10⁷ | [2²⁴, 2²⁵) | 24 | **2** |
| `sum2` | 2.17×10¹⁰ | [2³⁴, 2³⁵) | 34 | 2¹¹ = **2048** |

Key readings:

- `mean` in f32 is essentially perfect (±0.0005 ADU) → **pedestal subtraction is
  not the problem.**
- `mean²` and `E[X²]` are representable only to the **nearest 2 ADU²**.
- The raw accumulator `sum2` is good only to the **nearest 2048** — its bottom
  ~4 decimal digits are noise (7-digit float holding an 11-digit number).

---

## 5. Error propagation through the subtraction

```
var = E[X²] − mean²
    = (21 671 050 ± 2) − (21 669 025 ± 2)     ← each operand on a grid of 2
    = 2025 ± ~3                                ← plus sum2/n quantization (±2)  →  ±3–4 ADU²
```

The result 2025 is finely representable, **but carries a ±3–4 ADU² error
inherited from the two giant operands**, and that error is a **fixed absolute
size — it does not shrink for quiet pixels.**

---

## 6. Absolute error → relative error explodes for quiet pixels

| pixel | true rms | true var | var in f32 | rms error | 5σ threshold effect |
|---|---|---|---|---|---|
| noisy (bulk) | 45 | 2025 | 2025 ± 3 | 0.07 % | 225.0 → 225.1 — harmless |
| moderate | 10 | 100 | 100 ± 3 | 1.5 % | 50 → 50.8 |
| quiet | 5 | 25 | 25 ± 3 | 6 % | 25 → 26.5 |
| very quiet | 3 | 9 | 9 ± 3 | 17 % | 15 → 12–17 |
| near-flat | 2 | 4 | **4 ± 3 → can be ≤ 0** | **∞** | **clamp → 0** |

The bulk of the detector is fine (why the spectrum mostly looks right), but every
low-noise pixel has a corrupted threshold, and the flattest fall off the cliff.

---

## 7. The clamp turns "wrong" into "catastrophic"

`rms_sq = (var_px > 0) ? var_px : 0`

When cancellation drives a quiet pixel's `var_px` to **≤ 0**, it clamps to
`rms_sq = 0` → `nSig_sq_rms_sq = 0` → Test 1 becomes `max_val² > 0`. The pixel is
declared a photon on **any** positive fluctuation: its 5σ gate (~225 ADU) has
become a 0σ gate, so it fires essentially every frame. (The clamp is not a bug —
it exists *because* negative variances were already occurring; it just converts
them into hot pixels rather than NaNs.)

---

## 8. Order-of-magnitude count check (the +28 %)

Measured excess: **+32.5M** clusters over **20 000** frames:

```
32.5×10⁶ / 20 000 ≈ 1600 extra clusters per frame
```

A stuck (threshold-0) pixel yields ~1 cluster/frame (whenever it is the local max
of its window). So this matches **~1600–3000 corrupted pixels out of 160 000 —
about 1–2 % of the array** having low enough noise to be underestimated/clamped
by the ±3 ADU² f32 error. That accounts for the whole 28 %.

---

## 9. Why the tail is *high*-energy

A correct 5σ threshold (~225 ADU) removes everything below it — the spectrum
starts at ~225. A stuck pixel with threshold ≈ 0 admits the **entire positive
side** of its distribution. Its cluster energy is the window sum
`Σ(raw − mean)`, and with the gate wide open it accepts ordinary positive noise
(low energy) **plus** the occasional large excursion or real charge drifting
through that a proper cut would remove. Instead of a clean edge at 225 you get a
population smeared from ~0 upward — an unphysical **tail**. Same corrupted pixels
produce both the count excess and the tail.

---

## 10. Compounding effect: the running update

The per-frame EMA update (~lines 293–294) runs in the same f32:

```cpp
sum2 += raw*raw − sum2/n;
```

`raw² ≈ 21.67M` (ULP 2); the increment fluctuates by `±2·mean·δ ≈ ±2·4655·45 ≈
±419 000`; it is added to `sum2 ≈ 2.17×10¹⁰` (ULP 2048). Each update therefore
rounds by up to ±1024 → injects ~±1 ADU² of jitter into `E[X²]` every frame, on
top of the static cancellation error. Over thousands of frames the EMA stays
bounded but permanently noisy, nudging borderline pixels across the clamp.

Also relevant: `sync_pedestal_to_device` (in `ClusterFinderCUDA.hpp`) casts the
host's **double** `sum2` **down to f32 on upload**, so in the f32 build the
precision is destroyed *before the first data frame*.

---

## 11. Why double precision makes it vanish

Same formula, `DEVICE_PED_TYPE = double` → 52 mantissa bits ≈ 15–16 decimal
digits. ULP at `2.17×10⁷` is `2²⁴⁻⁵² = 2⁻²⁸ ≈ 4×10⁻⁹ ADU²`:

```
var = (21 671 050 ± 4e-9) − (21 669 025 ± 4e-9) = 2025 ± 1e-8
```

Variance is exact for all practical purposes, no pixel is ever clamped, and the
host→device cast is a no-op → CPU and CUDA agree to 0.0001 %.

---

## 12. Fix options

| option | what | pro | con |
|---|---|---|---|
| **A. f64 pedestal** | `DEVICE_PED_TYPE = double` | trivially correct (proven) | 2× pedestal memory + bandwidth; loses the f32 perf win |
| **B1. per-pixel offset** | accumulate `X − X0` with `X0 ≈ round(mean)` per pixel; keep `sum`/`sum2` of the *centered* values | keeps f32 bandwidth **and** correctness; small kernel + host-sync change | needs a per-pixel offset array and one-time rebase |
| **B2. Welford** | store running `mean` + `M2` directly; never form `E[X²]` | numerically ideal; no offset bookkeeping | more work per update; larger change to the update step |

**Why B works:** with an offset `X0 ≈ 4655`, the accumulated quantities become
O(45) and O(2025) instead of O(4655) and O(2×10⁷), so `var = sum2'/n − mean'²`
is a difference of ~2000-scale numbers → f32 resolves it to ~1e-4 relative. The
cancellation is gone and the variance no longer depends on the absolute pedestal
level.

**Recommendation:** **B1 (per-pixel offset)** — recovers the f32 performance
without the errors. B2 (Welford) is the cleanest if a larger refactor of the
update path is acceptable.

---

## References

- Kernel: `include/aare/clusterfinder_kernel.cuh` — variance ~L186–191, clamp
  L190, running update ~L293–298.
- Host sync / f32 cast: `include/aare/ClusterFinderCUDA.hpp` —
  `sync_pedestal_to_device`.
- Validation: `python/tests/ClusterFinderFrozen_vs_CUDA.ipynb` (f64 build,
  ~100 % concordance) and `python/tests/ClusterFinderCUDA_perf.ipynb` (f32
  pedestal build, +28 % + tail).
