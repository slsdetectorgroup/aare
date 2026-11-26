# SPDX-License-Identifier: MPL-2.0
"""
Fit a cube of S-curves in parallel
==================================

Fit one S-curve per pixel of a threshold scan with ``aare.FallingScurve``.
The pixels are independent, so the fits are spread over several threads with
``n_threads``. See :doc:`/pyFit` for the other models and the available
minimizers.
"""

import matplotlib.pyplot as plt
import numpy as np

from aare import FallingScurve, add_colorbar

# %%
# .. rubric:: Generate test data
#
# In a threshold scan of a photon counting detector the number of counts
# drops from a plateau to the background level when the threshold passes the
# photon energy. ``FallingScurve`` describes this with six parameters:
#
# .. math::
#
#     f(x) = p_0 + p_1 x + \frac{1}{2}
#            \left[1 - \mathrm{erf}\left(\frac{x - \mu}{\sqrt{2}\sigma}\right)
#            \right] \left[A + C (x - \mu)\right]
#
# where :math:`\mu` is the inflection point, :math:`\sigma` the noise,
# :math:`A` the height of the plateau extrapolated to :math:`\mu`,
# :math:`C` the slope of the plateau from charge sharing and
# :math:`p_0 + p_1 x` a linear background.
#
# Generate a scan with 80 thresholds from 0 to 2000 for a 64 x 64 pixel
# detector. A model object is callable, which we use to evaluate the
# expected counts of each pixel before adding counting noise.

rows, cols = 64, 64
threshold = np.linspace(0.0, 2000.0, 80)

scurve = FallingScurve()
print(scurve.par_names)

row, col = np.mgrid[:rows, :cols]
truth = np.empty((rows, cols, len(scurve.par_names)))
truth[:, :, 0] = 5.0  # p0
truth[:, :, 1] = 0.0  # p1
# The position of mu varies with the distance from the centre of the image
truth[:, :, 2] = 1000.0 + 100.0 * np.hypot(row - rows / 2, col - cols / 2) / rows
truth[:, :, 3] = 100.0  # sigma
truth[:, :, 4] = 500.0  # A
truth[:, :, 5] = -0.1  # C

expected = np.empty((rows, cols, threshold.size))
for r in range(rows):
    for c in range(cols):
        expected[r, c] = scurve(threshold, truth[r, c])

rng = np.random.default_rng(42)
counts = rng.poisson(expected)

# %%
# .. rubric:: Fit the data cube
#
# The fit takes three arrays:
#
# - ``x`` holds the scan points, here the thresholds, with the shape
#   ``(scan_points,)``. All pixels share the same scan points.
# - ``y`` is the data cube with the measured counts of every pixel and the
#   shape ``(rows, columns, scan_points)``, so ``y[row, col]`` is the scan
#   of a single pixel.
# - ``y_err`` holds the uncertainty of every value in ``y`` and has the same
#   shape. For counting data it is the square root of the counts. Points
#   with ``y_err == 0`` are left out of the fit.
#
# The fit works on ``float64``, so convert the counts once here to avoid a
# copy of the whole cube inside ``fit``.

x = threshold
y = counts.astype(np.float64)
y_err = np.sqrt(y)

print(f"{x.shape = }")
print(f"{y.shape = }")
print(f"{y_err.shape = }")

# %%
# Create the model and fit all pixels with a single call. ``n_threads`` sets
# the number of worker threads that share the pixels. Start values are
# estimated for each pixel from its data.

model = FallingScurve(compute_errors=True)
result = model.fit(x, y, y_err, n_threads=8)

# %%
# For a cube the result holds one set of parameters per pixel: ``par`` and
# ``par_err`` have the shape ``(rows, columns, npar)`` and ``chi2`` has the
# shape ``(rows, columns)``.

for key, value in result.items():
    print(f"{key:>7}: {value.shape}")

# %%
# .. rubric:: Inspect the result
#
# Compare the fitted parameters with the ones used to generate the data.

residual = result["par"] - truth

for i, name in enumerate(model.par_names):
    print(
        f"{name:>5}: mean deviation = {residual[:, :, i].mean():7.3f}, "
        f"spread = {residual[:, :, i].std():.3f}, "
        f"mean error = {result['par_err'][:, :, i].mean():.3f}"
    )

# %%
# Each parameter is an image. Show the fitted inflection point next to the
# reduced :math:`\chi^2` of the fits.

i_mu = model.par_names.index("mu")
ndf = x.size - len(model.par_names)

fig, axs = plt.subplots(1, 2, figsize=(9, 4), constrained_layout=True)

im = axs[0].imshow(result["par"][:, :, i_mu])
axs[0].set_title("fitted mu")
add_colorbar(axs[0], im)

im = axs[1].imshow(result["chi2"] / ndf)
axs[1].set_title("chi2 / ndf")
add_colorbar(axs[1], im)

for ax in axs:
    ax.set_xlabel("column")
    ax.set_ylabel("row")

# %%
# Evaluate the model with the parameters of a single pixel to draw its fit
# on top of the data.

r, c = 10, 20
x_fine = np.linspace(x[0], x[-1], 500)
y_fine = model(x_fine, result["par"][r, c])

fig, ax = plt.subplots()
ax.errorbar(x, y[r, c], y_err[r, c], fmt=".", label=f"pixel ({r}, {c})")
ax.plot(x_fine, y_fine, label="fit")
ax.set_xlabel("threshold")
ax.set_ylabel("counts")
ax.grid()
ax.legend()
plt.show()
