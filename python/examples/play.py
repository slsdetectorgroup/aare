import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

from aare._aare import ClusterVector_i, Interpolator

import pickle 
import numpy as np
import matplotlib.pyplot as plt
import boost_histogram as bh
import torch
import math
import time



def gaussian_2d(mx, my, sigma = 1, res=100, grid_size = 2):
    """
    Generate a 2D gaussian as position mx, my, with sigma=sigma. 
    The gaussian is placed on a 2x2 pixel matrix with resolution 
    res in one dimesion.
    """
    x = torch.linspace(0, pixel_size*grid_size, res)
    x,y = torch.meshgrid(x,x, indexing="ij")
    return 1 / (2*math.pi*sigma**2) * \
      torch.exp(-((x - my)**2 / (2*sigma**2) + (y - mx)**2 / (2*sigma**2)))

scale = 1000 #Scale factor when converting to integer
pixel_size = 25 #um
grid = 2
resolution = 100
sigma_um = 10
xa = np.linspace(0,grid*pixel_size,resolution)
ticks = [0, 25, 50]

hit = np.array((20,20))
etahist_fname = "/home/l_msdetect/erik/tmp/test_hist.pkl"

local_resolution = 99
grid_size = 3
xaxis = np.linspace(0,grid_size*pixel_size, local_resolution)
t = gaussian_2d(hit[0],hit[1], grid_size = grid_size, sigma = 10, res = local_resolution)
pixels = t.reshape(grid_size, t.shape[0] // grid_size, grid_size, t.shape[1] // grid_size).sum(axis = 3).sum(axis = 1)
pixels = pixels.numpy()
pixels = (pixels*scale).astype(np.int32)
v = ClusterVector_i(3,3)
v.push_back(1,1, pixels)

with open(etahist_fname, "rb") as f:
        hist = pickle.load(f)
eta = hist.view().copy()
etabinsx = np.array(hist.axes.edges.T[0].flat)
etabinsy = np.array(hist.axes.edges.T[1].flat)
ebins = np.array(hist.axes.edges.T[2].flat)
p = Interpolator(eta, etabinsx[0:-1], etabinsy[0:-1], ebins[0:-1])




#Generate the hit




tmp = p.interpolate(v)
print(f'tmp:{tmp}')
pos = np.array((tmp['x'], tmp['y']))*25


print(pixels)
fig, ax = plt.subplots(figsize = (7,7))
ax.pcolormesh(xaxis, xaxis, t)
ax.plot(*pos, 'o')
ax.set_xticks([0,25,50,75])
ax.set_yticks([0,25,50,75])
ax.set_xlim(0,75)
ax.set_ylim(0,75)
ax.grid()
print(f'{hit=}')
print(f'{pos=}')