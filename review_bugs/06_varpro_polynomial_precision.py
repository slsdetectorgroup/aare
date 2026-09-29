"""VarPro loses precision on polynomials when x is offset from zero.

Pol1/Pol2 are fully linear, so VarPro solves them in one step from the
normal equations Phi^T W Phi with a Cholesky factorisation. That squares the
condition number of the basis [1, x, x^2], which is large when the scan
range is narrow compared to its offset. LM and Migrad reach ~1e-11; VarPro
was off by 5.5e-7 relative at x = 20000 and still reported a valid fit. From
about x = 60000 its collinearity check falls back to LM.

Code: src/fit/VariableProjection.hpp reduce(), pass 1 and cholesky_factor.
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
worst = 0.0
for lo, span in [(8000, 1000), (20000, 500), (60000, 200)]:
    x = np.linspace(lo, lo + span, 40)
    y = 1.5 + 2e-3 * (x - lo) + 3e-7 * (x - lo) ** 2
    ref = np.polyfit(x, y, 2)[::-1]
    print(f"x in [{lo}, {lo + span}]")
    for m in [M.Migrad, M.LevenbergMarquardt, M.VarPro]:
        par = aare.Pol2(minimizer=m).fit(x, y)["par"]
        rel = np.max(np.abs(par - ref) / np.abs(ref))
        print(f"  {m.name:20s} max relative error vs np.polyfit = {rel:.1e}")
        if m == M.VarPro:
            worst = max(worst, rel)

if worst > 1e-9:
    print(f"\nBUG REPRODUCED: VarPro relative error {worst:.1e}")
    sys.exit(1)
print("\nnot reproduced")
