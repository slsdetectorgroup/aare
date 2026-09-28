"""A step reflected at a limit back onto the start point is taken as
convergence.

Fit a constant (Pol1 with p1 fixed at 0) to y = 2.2 with p0 limited to
[-1, 1] and started at 0. The damped step is +2; reflected at the upper
limit 1 it lands back on 0, the step length is 0, and the driver stops with
"success" before evaluating the truncated step (p0 = 1). The constrained
optimum is p0 = 1, chi2 = 5.76; LM and VarPro return p0 = 0, chi2 = 19.36.

Code: src/DampedGaussNewton.hpp:269 (small-step test before the trial).
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.arange(4.0)  # [0.   1.   2.   3. ]
y = np.full(4, 2.2) # [2.2  2.2  2.2  2.2]

print("expected: p0 = 1, chi2 = 5.76")
wrong = []
for m in [M.Migrad, M.Fumili, M.LevenbergMarquardt, M.VarPro]:
    model = aare.Pol1(minimizer=m)
    model.SetParameter("p0", 0.0)
    model.FixParameter("p1", 0.0)
    model.SetParLimits("p0", -1.0, 1.0)
    r = model.fit(x, y)
    print(f"{m.name:20s} p0 = {r['par'][0]:.6g}  chi2 = {r['chi2'][0]:.6g}")
    if abs(r["par"][0] - 1.0) > 1e-6:
        wrong.append(m.name)

if wrong:
    print(f"\nBUG REPRODUCED: false convergence in {', '.join(wrong)}")
    sys.exit(1)
print("\nnot reproduced")
