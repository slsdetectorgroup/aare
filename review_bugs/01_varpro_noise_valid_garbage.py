"""VarPro (and occasionally LM) return absurd parameters on noise pixels and
report them as valid fits.

Fit a Gaussian to 400 pixels of pure noise. Migrad either finds a small
spurious peak or reports a failure (all zeros). VarPro never reports a
failure; instead sigma collapses until the basis function is ~0 on every
scan point and the linear solve for the amplitude explodes (|A| up to 1e159).
Because the fit is "valid", the documented VarPro -> LM fallback never runs.

Code: src/VariableProjection.hpp reduce() (Cholesky only checks pivot > 0),
src/DampedGaussNewton.hpp degenerate() (compares with the previous point only).
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.linspace(-6, 6, 61)
cube = np.random.default_rng(0).normal(0, 1, (10, 10, x.size))

print(f"{'minimizer':20s} {'failed':>6s} {'|A|>1e6 valid':>14s} {'max |A|':>10s}")
bad = {}
for m in [M.Migrad, M.Fumili, M.LevenbergMarquardt, M.VarPro]:
    par = aare.Gaussian(minimizer=m).fit(x, cube, n_threads=4)["par"]
    if(m == M.Migrad):
        print(par)
    failed = np.all(par == 0, axis=-1)
    absurd = (np.abs(par[..., 0]) > 1e6) & ~failed
    bad[m] = absurd.sum()
    print(f"{m.name:20s} {failed.sum():6d} {absurd.sum():14d} "
          f"{np.abs(par[..., 0]).max():10.3g}")

if bad[M.VarPro] or bad[M.LevenbergMarquardt]:
    print("\nBUG REPRODUCED: absurd amplitudes reported as valid fits")
    sys.exit(1)
print("\nnot reproduced")
