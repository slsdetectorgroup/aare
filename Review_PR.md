# Review: dev/multifit-opt into main

Reviewed branch: db2f6455805452bf7f670b93055dc43ee141f0b1.
Base: 941325ea151e627f61c9e732e34a751ecfffa510 (main is an ancestor).

Recommendation: fix the findings below before merging. They reproduce through the public fitting functions; the new LevenbergMarquardt and VarPro paths are affected. Migrad and Fumili pass the four targeted examples.

## 1. Reflected zero step incorrectly signals convergence

Location: src/DampedGaussNewton.hpp:269.

Fit Pol1 to x = [0,1,2,3], y = [2.2,2.2,2.2,2.2]; fix p1=0, start p0=0, limit p0 to [-1,1]. The damped step reflects back to the starting point. The small-step check returns success before evaluating the alternative truncated step. LM and VarPro return p0=0, chi2=19.36. The constrained optimum is p0=1, chi2=5.76.

Evaluate the truncated alternative before concluding convergence and require an appropriate optimality check. The current global relative step norm also fails a translated-Gaussian test: adding 1e9 to every x coordinate causes both solvers to stop at sigma=1.361702 instead of recovering sigma=1.3 (the same unshifted data fits correctly). Use appropriately scaled parameters and an optimality check instead of accepting stagnation alone.

## 2. Rejected trial incorrectly freezes a parameter of the accepted point

Location: src/DampedGaussNewton.hpp:364.

mark_active() uses p_ from the last trial with the gradient from the accepted point. After both boundary candidates are rejected, it can freeze an interior parameter and conclude that the accepted point is optimal.

Reproducer: x=linspace(-0.1,0.1,11), y=10*exp(-0.5*(x-sqrt(2*log(2)))**2). Fit Gaussian with A=10 and sigma=1 fixed, mu starting at 3, mu limits [-3,5]. LM and VarPro return mu=3, chi2=264.488158684; Migrad/Fumili recover mu approximately 1.17741 with chi2 near zero.

Determine active constraints at the accepted point and use its matching gradient. Recheck convergence and error computation after rejected trials.

## 3. Zero-error samples do not actually mask NaNs

Locations: src/LevenbergMarquardt.hpp:130 and src/VariableProjection.hpp:247 (also subsequent reductions).

Fit Pol1 to x=arange(10), y=2+0.5*x, errors all 1, with y[4]=NaN and error[4]=0. Migrad/Fumili skip the masked sample and return [2,0.5], chi2=0. LM returns chi2=NaN; VarPro returns NaN parameters and chi2. Multiplying by zero does not remove a NaN. The driver can classify a NaN gradient as converged because std::max retains its finite first operand.

Skip zero-weight samples before model/data arithmetic, and reject non-finite objective/gradient/Hessian values instead of returning successful results.

## 4. Errors at limits disagree with the documented contract

Locations: src/DampedGaussNewton.hpp:321 and src/VariableProjection.hpp:407.

Fit Pol1 with compute_errors=True to x=[-1,-1,1,1], y=1+2*x, p0 limited to [0,1]. Both new solvers return p0=1 and error(p0)=0.5, although the documented contract specifies zero. Migrad/Fumili return zero. LM only skips constraints with an outward gradient; at an exact optimum the gradient is zero. VarPro does not check limits of free linear parameters in its error mask at all.

Build the error mask from the final parameter values and limits, including the linear parameters, consistently across minimizers.

## Validation artifacts

- Build with GCC 13.4, Python 3.12.5 and the branch's Minuit2/pybind11 versions succeeded.
- Existing C++ fitting suite: 18 cases, 2571 assertions passed.
- Full CTest suite: 259 tests passed.
- Existing Python fitting tests: 56 passed.
- Full Python suite: 401 passed, 28 skipped. pytest-check was installed only into this temporary review directory.
- Targeted Python regression tests: 10 failed, 4 passed, confirming the findings plus the coordinate-scaling case.
- External detector-data tests were not enabled; AARE_TEST_DATA is unavailable.
- Branch diff passes git diff --check.
- repro_fits.cpp and repro_fits.log: public C++ API reproductions for all four minimizers.
- review_regressions.py: equivalent Python regression tests, plus a translated-Gaussian check.
- fit-tests.log, ctest.log, build.log: build/test output.

Additional coverage worth adding: end-to-end fits of Pol2, GaussianErfcPlateau, GaussianChargeSharing and GaussianChargeSharingKb. Current correctness tests check their basis functions but do not fit these models end to end. Test mixed successful/failed pixels in one cube to exercise state reuse, and real detector data before making performance claims.

The original repository and branch were not modified; these files are in an isolated snapshot under /tmp.
