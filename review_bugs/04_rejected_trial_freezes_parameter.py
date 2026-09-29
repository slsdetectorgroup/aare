"""After a rejected trial, a parameter of the accepted (interior) point is
frozen as if it sat on a limit, and the fit stops there.

mark_active() tests the last *evaluated* point p_ (the rejected trial, which
was truncated onto the limit) against the gradient of the *accepted* point.
The interior parameter mu is then frozen, the EDM over the remaining free
parameters is 0, and the fit "converges" at the start value mu = 3.

The lower limit is 0: with x covering only [-0.1, 0.1] the problem also has
a mirror minimum near mu = -1.18 (chi2 6.07), which a fit that is not stuck
can legitimately reach when the limit allows it (the review's original
limit of -3 does).

Code: src/DampedGaussNewton.hpp:364 (mark_active).
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.linspace(-0.1, 0.1, 11)
mu_true = np.sqrt(2 * np.log(2))
y = 10 * np.exp(-0.5 * (x - mu_true) ** 2)

print(f"expected: mu = {mu_true:.5f}, chi2 ~ 0")
wrong = []
for m in [M.Migrad, M.Fumili, M.LevenbergMarquardt, M.VarPro]:
    model = aare.Gaussian(minimizer=m)
    model.FixParameter("A", 10.0)
    model.FixParameter("sigma", 1.0)
    model.SetParameter("mu", 3.0)
    model.SetParLimits("mu", 0.0, 5.0)
    r = model.fit(x, y)
    print(f"{m.name:20s} mu = {r['par'][1]:.5f}  chi2 = {r['chi2'][0]:.6g}")
    if abs(r["par"][1] - mu_true) > 1e-3:
        wrong.append(m.name)

if wrong:
    print(f"\nBUG REPRODUCED: fit stuck at start in {', '.join(wrong)}")
    sys.exit(1)
print("\nnot reproduced")
