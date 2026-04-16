import pytest 

from aare import Interpolator, ClusterVector, Etai, Cluster

import numpy as np 


def test_interpolation_api():
    eta_distribution = np.zeros((10, 10, 1)) # dummy eta distribution 
    etax_bins = np.linspace(0, 1.0, 11) 
    etay_bins = np.linspace(0, 1.0, 11)
    e_bins = np.array([0., 10.]) # dummy energy bins
    interpolator = Interpolator(eta_distribution, etax_bins, etay_bins, e_bins)

    cluster_vector = ClusterVector()
    cluster_vector.push_back(Cluster(10, 5, np.ones(shape=9, dtype=np.int32)))
    cluster_vector.push_back(Cluster(20, 10, np.ones(shape=9, dtype=np.int32)))

    eta1 = Etai()
    eta1.x = 0.1
    eta1.y = 0.1
    eta1.sum = 5
    eta2 = Etai()
    eta2.x = 0.1
    eta2.y = 0.9
    eta2.sum = 6
    etas = np.array([eta1, eta2]) # dummy etas for the clusters

    photons = interpolator.interpolate(cluster_vector, etas)

    assert photons.size == 2