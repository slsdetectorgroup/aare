"""NaN samples: masking with a zero error does not work, and an unmasked NaN
is not reported as a failure.

Case 1, masked (y = NaN with error 0, documented as "ignored"): Migrad and
Fumili skip the sample; LM returns chi2 = NaN, VarPro NaN parameters.
0 * NaN is NaN, so the weight does not remove the sample.

Case 2, unmasked (y = NaN, no errors, e.g. a dead frame): Migrad and Fumili
report a failed fit (zeros); LM returns plausible parameters with chi2 = NaN
and VarPro a NaN amplitude, both without signalling failure.

Code: src/LevenbergMarquardt.hpp:130, src/VariableProjection.hpp:247, and
std::max(gmax_, NaN) in src/DampedGaussNewton.hpp:352 hiding NaN gradients.
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
ALL = [M.Migrad, M.Fumili, M.LevenbergMarquardt, M.VarPro]
problems = []

print("case 1: Pol1, y[4] = NaN with error 0 (expected par [2, 0.5], chi2 0)")
x = np.arange(10.0)
y = 2 + 0.5 * x
s = np.ones(10)
y[4], s[4] = np.nan, 0.0
for m in ALL:
    r = aare.Pol1(minimizer=m).fit(x, y, s)
    print(f"  {m.name:20s} par = {r['par']}  chi2 = {r['chi2'][0]}")
    if not np.all(np.isfinite(r["par"])) or not np.isfinite(r["chi2"][0]):
        problems.append(f"{m.name} (masked)")

print("\ncase 2: Gaussian, y[10] = NaN, no errors (expected: failure = zeros)")
x = np.linspace(-6, 6, 61)
y = 120 * np.exp(-0.5 * ((x - 0.8) / 1.3) ** 2)
y[10] = np.nan
for m in ALL:
    r = aare.Gaussian(minimizer=m).fit(x, y)
    print(f"  {m.name:20s} par = {r['par']}  chi2 = {r['chi2'][0]}")
    if np.any(r["par"] != 0) and not np.isfinite(r["chi2"][0]):
        problems.append(f"{m.name} (unmasked)")

if problems:
    print("\nBUG REPRODUCED: NaN results in " + ", ".join(problems))
    sys.exit(1)
print("\nnot reproduced")
