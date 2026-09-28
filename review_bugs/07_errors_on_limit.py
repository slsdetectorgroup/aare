"""A parameter that ends on a limit must report an error of 0 (documented
for every minimizer), but LM and VarPro report a nonzero error.

The optimum p0 = 1 lies exactly on the upper limit, where the gradient is
0. LM only skips parameters with an outward gradient; VarPro never checks
the limits of linear parameters when building the error mask.

Code: src/DampedGaussNewton.hpp:321 (skipped), src/VariableProjection.hpp:407.
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.array([-1.0, -1.0, 1.0, 1.0])
y = 1 + 2 * x

print("expected: p0 = 1 on its limit, error(p0) = 0")
wrong = []
for m in [M.Migrad, M.Fumili, M.LevenbergMarquardt, M.VarPro]:
    model = aare.Pol1(minimizer=m, compute_errors=True)
    model.SetParLimits("p0", 0.0, 1.0)
    r = model.fit(x, y)
    print(f"{m.name:20s} p0 = {r['par'][0]:.6g}  error(p0) = {r['par_err'][0]:.6g}")
    if r["par_err"][0] != 0.0:
        wrong.append(m.name)

if wrong:
    print(f"\nBUG REPRODUCED: nonzero error on a limit in {', '.join(wrong)}")
    sys.exit(1)
print("\nnot reproduced")
