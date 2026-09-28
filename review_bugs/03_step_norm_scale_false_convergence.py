"""The "negligible step" test uses one global relative norm, so one large
parameter hides the steps of all the others.

The driver stops when |step| <= 1e-8 * |q| over all free parameters. With
a large amplitude (weighted fit, A = 1e8 or 1e10) or a large x offset (mu =
1e9), steps in sigma look negligible from the start and LM stops early, at
A = 1e10 exactly at the start estimate sigma = 1.361702 instead of 1.3.
The fit is reported as valid.

Code: src/DampedGaussNewton.hpp:269.
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.linspace(-6, 6, 61)
g = np.exp(-0.5 * ((x - 0.8) / 1.3) ** 2)
wrong = []

print("case                     minimizer            mu - offset   sigma (true 1.3)")
for label, xs, amp, offset in [
    ("A = 1e8, weighted", x, 1e8, 0.0),
    ("A = 1e10, weighted", x, 1e10, 0.0),
    ("x shifted by 1e9", x + 1e9, 120.0, 1e9),
]:
    y = amp * g
    s = np.sqrt(y) + 1
    for m in [M.Migrad, M.LevenbergMarquardt, M.VarPro]:
        par = aare.Gaussian(minimizer=m).fit(xs, y, s)["par"]
        print(f"{label:24s} {m.name:20s} {par[1] - offset:11.6f}   {par[2]:.6f}")
        if abs(par[2] - 1.3) > 1e-3:
            wrong.append(f"{m.name} ({label})")

if wrong:
    print("\nBUG REPRODUCED: early stop in " + "; ".join(wrong))
    sys.exit(1)
print("\nnot reproduced")
