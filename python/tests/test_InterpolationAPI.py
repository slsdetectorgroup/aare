# SPDX-License-Identifier: MPL-2.0
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

    assert photons.size == cluster_vector.size # should return one photon per cluster


def test_state_file_restores_the_interpolator(tmp_path):
    rng = np.random.default_rng(0)
    eta_distribution = rng.uniform(1, 10, size=(10, 8, 2))
    etax_bins = np.linspace(0, 1.0, 11)
    etay_bins = np.linspace(0, 1.0, 9)
    e_bins = np.array([0.0, 50.0, 100.0])
    original = Interpolator(eta_distribution, etax_bins, etay_bins, e_bins)
    assert original.save_state(tmp_path / "interpolator.aare") is None

    restored = Interpolator.from_state_file(tmp_path / "interpolator.aare")
    # An interpolator with other bins and no distribution
    given = Interpolator(np.array([0.0, 1.0]), np.array([0.0, 1.0]), np.array([0.0, 1.0]))
    assert given.load_state(str(tmp_path / "interpolator.aare")) is None

    cluster_vector = ClusterVector()
    cluster_vector.push_back(Cluster(10, 5, np.array([1, 2, 2, 1, 4, 1, 1, 2, 1], dtype=np.int32)))
    cluster_vector.push_back(Cluster(20, 10, np.array([1, 2, 3, 3, 5, 1, 1, 2, 1], dtype=np.int32)))
    expected = original.interpolate(cluster_vector)
    for interpolator in (restored, given):
        np.testing.assert_array_equal(interpolator.get_ietax(), original.get_ietax())
        np.testing.assert_array_equal(interpolator.get_ietay(), original.get_ietay())
        np.testing.assert_array_equal(interpolator.interpolate(cluster_vector), expected)

    # Not the state file of an interpolator: the interpolator is unchanged
    np.save(tmp_path / "bins.npy", etax_bins)
    with pytest.raises(RuntimeError, match="not an aare state file"):
        given.load_state(tmp_path / "bins.npy")
    np.testing.assert_array_equal(given.get_ietax(), original.get_ietax())
