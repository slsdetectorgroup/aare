# SPDX-License-Identifier: MPL-2.0
import matplotlib.pyplot as plt
import numpy as np

from aare import Gaussian, Pol1

textpm = "±"
textmu = "μ"
textsigma = "σ"


# ================================= Gauss fit =================================
mu = np.random.uniform(1, 100)
sigma = np.random.uniform(4, 20)
data = np.random.normal(mu, sigma, 10000)
counts, edges = np.histogram(data, bins=100)

x = 0.5 * (edges[:-1] + edges[1:])
y = counts.astype(np.float64)
yerr = np.sqrt(np.maximum(y, 1))

fig0, ax0 = plt.subplots(1, 1, num=0, figsize=(12, 8))
ax0.errorbar(x, y, yerr=yerr, fmt=". ", capsize=5)
ax0.grid()

gaussian = Gaussian(compute_errors=True)
result = gaussian.fit(x, y, yerr)
par = result["par"]
err = result["par_err"]
chi2 = result["chi2"]
print(f"Gaussian.fit: par={par}, err={err}, chi2={chi2}")

x_plot = np.linspace(x[0], x[-1], 1000)
ax0.plot(x_plot, gaussian(x_plot, par), marker="", label="Gaussian.fit")
ax0.legend()
ax0.set(
    xlabel="x",
    ylabel="Counts",
    title=(
        f"A={par[0]:0.2f}{textpm}{err[0]:0.2f}  "
        f"{textmu}={par[1]:0.2f}{textpm}{err[1]:0.2f}  "
        f"{textsigma}={par[2]:0.2f}{textpm}{err[2]:0.2f}\n"
        f"(truth: {textmu}={mu:0.2f}, {textsigma}={sigma:0.2f})"
    ),
)
fig0.tight_layout()


# ================================= Pol1 fit =================================
n_points = 40
slope = np.random.uniform(-10, 10)
intercept = np.random.uniform(-10, 10)
x_values = np.random.uniform(-10, 10, n_points)
errors = np.abs(np.random.normal(0, np.random.uniform(1, 5), n_points))
y_values = slope * x_values + intercept + np.random.normal(0, 1, n_points)

fig1, ax1 = plt.subplots(1, 1, num=1, figsize=(12, 8))
ax1.errorbar(x_values, y_values, yerr=errors, fmt=". ", capsize=5)

pol1 = Pol1(compute_errors=True)
result = pol1.fit(x_values, y_values, errors)
par = result["par"]
err = result["par_err"]

x_plot = np.linspace(np.min(x_values), np.max(x_values), 1000)
ax1.plot(x_plot, pol1(x_plot, par), marker="")
ax1.set(
    xlabel="x",
    ylabel="y",
    title=(
        f"intercept = {par[0]:0.2f}{textpm}{err[0]:0.2f}\n"
        f"slope = {par[1]:0.2f}{textpm}{err[1]:0.2f}\n"
        f"(truth: {intercept:0.2f}, {slope:0.2f})"
    ),
)
fig1.tight_layout()

plt.show()
