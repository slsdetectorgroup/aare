"""LM can declare convergence at the start point without taking a step.

The EDM tolerance is absolute in chi2 units (Minuit's convention). For an
unweighted fit of small-amplitude data the EDM at the start estimate is
already below 0.002 * tolerance, and LM returns the start estimate
(sigma = 1.361702) as the result. Migrad has the same convention and is
biased here too, but it always iterates at least once and moves.

With max_calls = 1 the driver can only evaluate the start point, so a valid
result proves that no step was taken.

Code: src/DampedGaussNewton.hpp:226 (EDM test before the first step).
"""
import sys; sys.path.append('../build')

import numpy as np

import aare

M = aare.Minimizer
x = np.linspace(-6, 6, 61)
g = np.exp(-0.5 * ((x - 0.8) / 1.3) ** 2)

print("unweighted Gaussian, true sigma = 1.3")
zero_step = False
for amp in [120, 1, 0.1, 0.03]:
    for m in [M.Migrad, M.LevenbergMarquardt]:
        par = aare.Gaussian(minimizer=m).fit(x, amp * g)["par"]
        print(f"  A = {amp:<5g} {m.name:20s} sigma = {par[2]:.6f}")
    one_call = aare.Gaussian(minimizer=M.LevenbergMarquardt, max_calls=1)
    par = one_call.fit(x, amp * g)["par"]
    if np.any(par != 0):
        zero_step = True
        print(f"  A = {amp:<5g} LM with max_calls=1 is valid: sigma = {par[2]:.6f}"
              " (start value, no step taken)")

if zero_step:
    print("\nBUG REPRODUCED: LM converged without taking a step")
    sys.exit(1)
print("\nnot reproduced")
