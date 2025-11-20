
#test script to test interpolation on simulated data 

import pytest
import pytest_check as check
import numpy as np 
import boost_histogram as bh 
import pickle
from scipy.stats import multivariate_normal

from aare import Interpolator, calculate_eta2, calculate_cross_eta3, calculate_full_eta2, calculate_eta3
from aare import ClusterFile

from conftest import test_data_path

## TODO: is there something like a test fixture setup/teardown in pytest?


def calculate_eta_distribution(cv, calculate_eta, edges_x=[-0.5,0.5], edges_y=[-0.5,0.5], nbins = 101):
    energy_bins = bh.axis.Regular(1, 0, 16) # max and min energy of simulated photons
    
    eta_distribution = bh.Histogram(
    bh.axis.Regular(nbins, edges_x[0], edges_x[1]), 
    bh.axis.Regular(nbins, edges_y[0], edges_y[1]), energy_bins) 

    eta = calculate_eta(cv)

    eta_distribution.fill(eta['x'], eta['y'], eta['sum'])

    return eta_distribution


@pytest.fixture
def load_data(test_data_path):
    """Load simulated cluster data and ground truth positions""" 
    f = ClusterFile(test_data_path / "clust" /  "simulated_clusters.clust", dtype=np.float64, mode="r")
    cv = f.read_frame() 

    ground_truths = np.load(test_data_path / "interpolation/ground_truth_simulated.npy")

    return cv, ground_truths
    
@pytest.mark.withdata
def test_eta2_interpolation(load_data, check):
    """Test eta2 interpolation on simulated data""" 

    cv, ground_truths = load_data

    num_bins = 201
    eta_distribution = calculate_eta_distribution(cv, calculate_eta2, edges_x=[-0.1,1.1], edges_y=[-0.1,1.1], nbins=num_bins) 

    interpolator = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges)

    assert interpolator.get_ietax().shape == (num_bins,num_bins,1)
    assert interpolator.get_ietay().shape == (num_bins,num_bins,1)

    interpolated_photons = interpolator.interpolate(cv)

    assert interpolated_photons.size == cv.size

    interpolated_photons["x"] += 1.0 #groud truth label uses 5x5 clusters 
    interpolated_photons["y"] += 1.0

    residuals_interpolated_x = abs(ground_truths[:, 0] - interpolated_photons["x"])
    residuals_interpolated_y = abs(ground_truths[:, 1] - interpolated_photons["y"])

    """
    residuals_center_pixel_x = abs(ground_truths[:, 0] - 2.5)
    residuals_center_pixel_y = abs(ground_truths[:, 1] - 2.5)

    # interpolation needs to perform better than center pixel assignment - not true for photon close to the center 
    assert (residuals_interpolated_x < residuals_center_pixel_x).all()
    assert (residuals_interpolated_y < residuals_center_pixel_y).all()
    """ 

    # check within photon hit pixel for all
    with check:
        assert np.allclose(interpolated_photons["x"], ground_truths[:, 0], atol=5e-1)
    with check:
        assert np.allclose(interpolated_photons["y"], ground_truths[:, 1], atol=5e-1)

    # check mean and std of residuals
    with check:
        assert residuals_interpolated_y.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.std() <= 0.05
    with check:
        assert residuals_interpolated_y.std() <= 0.05

@pytest.mark.withdata
def test_eta2_interpolation_rosenblatt(load_data, check):
    """Test eta2 interpolation on simulated data using Rosenblatt transform""" 

    cv, ground_truths = load_data

    num_bins = 201
    eta_distribution = calculate_eta_distribution(cv, calculate_eta2, edges_x=[-0.1,1.1], edges_y=[-0.1,1.1], nbins=num_bins) 

    interpolator = Interpolator(eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges)

    interpolator.rosenblatttransform(eta_distribution)

    assert interpolator.get_ietax().shape == (num_bins,num_bins,1)
    assert interpolator.get_ietay().shape == (num_bins,num_bins,1)

    interpolated_photons = interpolator.interpolate(cv)

    assert interpolated_photons.size == cv.size

    interpolated_photons["x"] += 1.0 #groud truth label uses 5x5 clusters 
    interpolated_photons["y"] += 1.0

    residuals_interpolated_x = abs(ground_truths[:, 0] - interpolated_photons["x"])
    residuals_interpolated_y = abs(ground_truths[:, 1] - interpolated_photons["y"])

    """
    residuals_center_pixel_x = abs(ground_truths[:, 0] - 2.5)
    residuals_center_pixel_y = abs(ground_truths[:, 1] - 2.5)

    # interpolation needs to perform better than center pixel assignment - not true for photon close to the center 
    assert (residuals_interpolated_x < residuals_center_pixel_x).all()
    assert (residuals_interpolated_y < residuals_center_pixel_y).all()
    """ 

    # check within photon hit pixel for all
    with check:
        assert np.allclose(interpolated_photons["x"], ground_truths[:, 0], atol=5e-1)
    with check:
        assert np.allclose(interpolated_photons["y"], ground_truths[:, 1], atol=5e-1)

    # check mean and std of residuals
    with check:
        assert residuals_interpolated_y.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.std() <= 0.055 #performs slightly worse
    with check:
        assert residuals_interpolated_y.std() <= 0.055 #performs slightly worse


@pytest.mark.withdata
def test_cross_eta_interpolation(load_data, check):
    """Test cross eta interpolation on simulated data""" 

    cv, ground_truths = load_data

    num_bins = 201
    eta_distribution = calculate_eta_distribution(cv, calculate_cross_eta3, edges_x=[-0.5,0.5], edges_y=[-0.5,0.5], nbins=num_bins) 

    interpolator = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges)

    assert interpolator.get_ietax().shape == (num_bins,num_bins,1)
    assert interpolator.get_ietay().shape == (num_bins,num_bins,1)

    interpolated_photons = interpolator.interpolate_cross_eta3(cv)

    assert interpolated_photons.size == cv.size

    interpolated_photons["x"] += 1.0 #groud truth label uses 5x5 clusters 
    interpolated_photons["y"] += 1.0

    residuals_interpolated_x = abs(ground_truths[:, 0] - interpolated_photons["x"])
    residuals_interpolated_y = abs(ground_truths[:, 1] - interpolated_photons["y"])

    """
    residuals_center_pixel_x = abs(ground_truths[:, 0] - 2.5)
    residuals_center_pixel_y = abs(ground_truths[:, 1] - 2.5)

    # interpolation needs to perform better than center pixel assignment - not true for photon close to the center 
    assert (residuals_interpolated_x < residuals_center_pixel_x).all()
    assert (residuals_interpolated_y < residuals_center_pixel_y).all()
    """ 

    # check within photon hit pixel for all
    # TODO: fails as eta_x = 0, eta_y = 0 is not leading to offset (0.5,0.5)
    with check:
        assert np.allclose(interpolated_photons["x"], ground_truths[:, 0], atol=5e-1)
    with check:
        assert np.allclose(interpolated_photons["y"], ground_truths[:, 1], atol=5e-1)

    # check mean and std of residuals
    with check:
        assert residuals_interpolated_y.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.std() <= 0.05
    with check:
        assert residuals_interpolated_y.std() <= 0.05

@pytest.mark.withdata
def test_eta3_interpolation(load_data, check):
    """Test eta3 interpolation on simulated data""" 

    cv, ground_truths = load_data

    num_bins = 201
    eta_distribution = calculate_eta_distribution(cv, calculate_eta3, edges_x=[-0.5,0.5], edges_y=[-0.5,0.5], nbins=num_bins) 

    interpolator = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges)

    assert interpolator.get_ietax().shape == (num_bins,num_bins,1)
    assert interpolator.get_ietay().shape == (num_bins,num_bins,1)

    interpolated_photons = interpolator.interpolate_eta3(cv)

    assert interpolated_photons.size == cv.size

    interpolated_photons["x"] += 1.0 #groud truth label uses 5x5 clusters 
    interpolated_photons["y"] += 1.0

    residuals_interpolated_x = abs(ground_truths[:, 0] - interpolated_photons["x"])
    residuals_interpolated_y = abs(ground_truths[:, 1] - interpolated_photons["y"])

    """
    residuals_center_pixel_x = abs(ground_truths[:, 0] - 2.5)
    residuals_center_pixel_y = abs(ground_truths[:, 1] - 2.5)

    # interpolation needs to perform better than center pixel assignment - not true for photon close to the center 
    assert (residuals_interpolated_x < residuals_center_pixel_x).all()
    assert (residuals_interpolated_y < residuals_center_pixel_y).all()
    """ 

    # check within photon hit pixel for all
    # TODO: fails as eta_x = 0, eta_y = 0 is not leading to offset (0.5,0.5)
    with check:
        assert np.allclose(interpolated_photons["x"], ground_truths[:, 0], atol=5e-1)
    with check:
        assert np.allclose(interpolated_photons["y"], ground_truths[:, 1], atol=5e-1)

    # check mean and std of residuals
    with check:
        assert residuals_interpolated_y.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.std() <= 0.05
    with check:
        assert residuals_interpolated_y.std() <= 0.05

@pytest.mark.withdata
def test_full_eta2_interpolation(load_data, check):
    """Test full eta2 interpolation on simulated data""" 

    cv, ground_truths = load_data

    num_bins = 201
    eta_distribution = calculate_eta_distribution(cv, calculate_full_eta2, edges_x=[-0.1,1.1], edges_y=[-0.1,1.1], nbins=num_bins) 

    interpolator = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges)

    assert interpolator.get_ietax().shape == (num_bins,num_bins,1)
    assert interpolator.get_ietay().shape == (num_bins,num_bins,1)

    interpolated_photons = interpolator.interpolate_full_eta2(cv)

    assert interpolated_photons.size == cv.size

    interpolated_photons["x"] += 1.0 #groud truth label uses 5x5 clusters 
    interpolated_photons["y"] += 1.0

    residuals_interpolated_x = abs(ground_truths[:, 0] - interpolated_photons["x"])
    residuals_interpolated_y = abs(ground_truths[:, 1] - interpolated_photons["y"])

    """
    residuals_center_pixel_x = abs(ground_truths[:, 0] - 2.5)
    residuals_center_pixel_y = abs(ground_truths[:, 1] - 2.5)

    # interpolation needs to perform better than center pixel assignment - not true for photon close to the center 
    assert (residuals_interpolated_x < residuals_center_pixel_x).all()
    assert (residuals_interpolated_y < residuals_center_pixel_y).all()
    """ 

    # check within photon hit pixel for all
    with check:
        assert np.allclose(interpolated_photons["x"], ground_truths[:, 0], atol=5e-1)
    with check:
        assert np.allclose(interpolated_photons["y"], ground_truths[:, 1], atol=5e-1)

    # check mean and std of residuals
    with check:
        assert residuals_interpolated_y.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.mean() <= 0.1
    with check:
        assert residuals_interpolated_x.std() <= 0.05
    with check:
        assert residuals_interpolated_y.std() <= 0.05
