# SPDX-License-Identifier: MPL-2.0
import matplotlib.pyplot as plt
import numpy as np
from aare import fit_gaus, fit_pol1, fit_gaus_minuit_grad
from aare import gaus, pol1

textpm = f"±"  #
textmu = f"μ"  #
textsigma = f"σ"  #



# ================================= Gauss fit =================================
# Parameters
mu = np.random.uniform(1, 100)  # Mean of Gaussian
sigma = np.random.uniform(4, 20)  # Standard deviation
num_points = 10000  # Number of points for smooth distribution
noise_sigma = 10

# Generate Gaussian distribution
data = np.random.normal(mu, sigma, num_points)

x = np.histogram(data, bins=30)[1][:-1] + 0.05
y_true = np.histogram(data, bins=30)[0]

# Generate errors for each point
yerr = np.abs(np.random.normal(0, noise_sigma, len(x)))

# Perturb the count wiht noise
y = y_true + yerr

# Create subplot
fig0, ax0 = plt.subplots(1, 1, num=0, figsize=(12, 8))

# Add the errors as error bars in the step plot
ax0.errorbar(x, y, yerr=yerr, fmt=". ", capsize=5)
ax0.grid()

# Fit with lmfit
result_lm = fit_gaus(x, y, yerr)
par_lm = result_lm["par"]
err_lm = result_lm["par_err"] 
print("fit_gaus:            ", par_lm, err_lm)

# Fit with Minuit2 + analytic gradient + Hesse errors
result_m2 = fit_gaus_minuit_grad(x, y, y_err=yerr, compute_errors=True)
par_m2 = result_m2[:3]
err_m2 = result_m2[3:]
print("fit_gaus_minuit_grad:", par_m2, err_m2)

x = np.linspace(x[0], x[-1], 1000)
ax0.plot(x, gaus(x, par_lm), marker="", label="fit_gaus")
ax0.plot(x, gaus(x, par_m2), marker="", linestyle=":", label="fit_gaus_minuit_grad")
ax0.legend()
ax0.set(xlabel="x", ylabel="Counts", 
    title=(
        f"fit_gaus:       A={par_lm[0]:0.2f}{textpm}{err_lm[0]:0.2f}  "
        f"{textmu}={par_lm[1]:0.2f}{textpm}{err_lm[1]:0.2f}  "
        f"{textsigma}={par_lm[2]:0.2f}{textpm}{err_lm[2]:0.2f}\n"
        f"minuit_grad: A={par_m2[0]:0.2f}{textpm}{err_m2[0]:0.2f}  "
        f"{textmu}={par_m2[1]:0.2f}{textpm}{err_m2[1]:0.2f}  "
        f"{textsigma}={par_m2[2]:0.2f}{textpm}{err_m2[2]:0.2f}\n"
        f"(truth: {textmu}={mu:0.2f}, {textsigma}={sigma:0.2f})"
    ),
)
fig0.tight_layout()



# ================================= pol1 fit =================================
# Parameters
n_points = 40

# Generate random slope and intercept (origin)
slope = np.random.uniform(-10, 10)  # Random slope between 0.5 and 2.0
intercept = np.random.uniform(-10, 10)  # Random intercept between -10 and 10

# Generate random x values
x_values = np.random.uniform(-10, 10, n_points)

# Calculate y values based on the linear function y = mx + b + error
errors = np.abs(np.random.normal(0, np.random.uniform(1, 5), n_points))
var_points = np.random.normal(0, np.random.uniform(0.1, 2), n_points)
y_values = slope * x_values + intercept + var_points

fig1, ax1 = plt.subplots(1, 1, num=1, figsize=(12, 8))
ax1.errorbar(x_values, y_values, yerr=errors, fmt=". ", capsize=5)
result_pol = fit_pol1(x_values, y_values, errors)
par = result_pol["par"]
err = result_pol["par_err"]

x = np.linspace(np.min(x_values), np.max(x_values), 1000)
ax1.plot(x, pol1(x, par), marker="")
ax1.set(xlabel="x", ylabel="y", title=f"a = {par[0]:0.2f}{textpm}{err[0]:0.2f}\n"
                                      f"b = {par[1]:0.2f}{textpm}{err[1]:0.2f}\n"
                                      f"(init: {slope:0.2f}, {intercept:0.2f})")
fig1.tight_layout()

plt.show()

