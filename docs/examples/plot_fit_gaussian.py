# SPDX-License-Identifier: MPL-2.0
"""
Fit a Gaussian
==============

Fit a Gaussian peak to a one-dimensional histogram with ``aare.Gaussian``.
See :doc:`/pyFit` for the other models and the available minimizers.
"""

import matplotlib.pyplot as plt
import numpy as np

from aare import Gaussian

# %%
# Generate a histogram of normally distributed values. The uncertainty of
# each bin is the square root of its content.

rng = np.random.default_rng(42)
samples = rng.normal(loc=25.0, scale=3.0, size=10_000)

counts, edges = np.histogram(samples, bins=60, range=(10, 40))
x = 0.5 * (edges[:-1] + edges[1:])
y = counts.astype(np.float64)
y_err = np.sqrt(y)

# %%
# Create the model and fit it. Start values are estimated from the data and
# empty bins, which have ``y_err == 0``, are left out of the fit.

model = Gaussian(compute_errors=True)
result = model.fit(x, y, y_err)

for name, par, err in zip(model.par_names, result["par"], result["par_err"]):
    print(f"{name:>5} = {par:7.2f} +- {err:.2f}")

# %%
# The model object is callable and evaluates the Gaussian for a set of
# parameters, which we use to draw the fitted curve on top of the data.

x_fine = np.linspace(x[0], x[-1], 500)

# Evaluate the fitted Gaussian at the points in x_fine
y_fine = model(x_fine, result["par"])

fig, ax = plt.subplots()
ax.errorbar(x, y, y_err, fmt=".", label="data")
ax.plot(x_fine, y_fine, label="fit")
ax.set_xlabel("x")
ax.set_ylabel("counts")
ax.grid()
ax.legend()
plt.show()
