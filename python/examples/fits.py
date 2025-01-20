import matplotlib.pyplot as plt
import numpy as np
from aare import fit_gaus2, fit_affine

textpm = f"±"  #
textmu = f"μ"  #
textsigma = f"σ"  #


def affine(x, a, b):
    return a * x + b


def gauss(x, a, x0, sigma):
    return a * np.exp(-0.5 * ((x - x0) / sigma)**2)


# ================================= Gauss fit =================================
# Parameters
mu = np.random.uniform(1, 100)  # Mean of Gaussian
sigma = np.random.uniform(4, 20)  # Standard deviation
num_points = 10000  # Number of points for smooth distribution
noise_sigma = 100

# Generate Gaussian distribution
data = np.random.normal(mu, sigma, num_points)

# Generate errors for each point
errors = np.abs(np.random.normal(0, sigma, num_points))  # Errors with mean 0, std 0.5

# Create subplot
fig0, ax0 = plt.subplots(1, 1, num=0, figsize=(12, 8))

x = np.histogram(data, bins=30)[1][:-1] + 0.05
y = np.histogram(data, bins=30)[0]
yerr = errors[:30]
# yerr = np.zeros_like(y)

# Add the errors as error bars in the step plot
ax0.errorbar(x, y, yerr=yerr, fmt=". ", capsize=5)
ax0.grid()

results = fit_gaus2(y, x, yerr)
print(results)

x = np.linspace(x[0], x[-1], 1000)
ax0.plot(x, gauss(x, *results[:3]), marker="")
ax0.set(xlabel="x", ylabel="Counts", title=f"A0 = {results[0]:0.2f}{textpm}{results[3]:0.2f}\n"
                                           f"{textmu} = {results[1]:0.2f}{textpm}{results[4]:0.2f}\n"
                                           f"{textsigma} = {results[2]:0.2f}{textpm}{results[5]:0.2f}\n"
                                           f"(init: {textmu}: {mu:0.2f}, {textsigma}: {sigma:0.2f})")
fig0.tight_layout()

# ================================= Affine fit =================================
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
results = fit_affine(y_values, x_values, errors)
print(results)

x = np.linspace(np.min(x_values), np.max(x_values), 1000)
ax1.plot(x, affine(x, *results[:2]), marker="")
ax1.set(xlabel="x", ylabel="y", title=f"a = {results[0]:0.2f}{textpm}{results[2]:0.2f}\n"
                                      f"b = {results[1]:0.2f}{textpm}{results[3]:0.2f}\n"
                                      f"(init: {slope:0.2f}, {intercept:0.2f})")
fig1.tight_layout()

plt.show()
