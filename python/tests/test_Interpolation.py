import pytest
import numpy as np 
import boost_histogram as bh 
import pickle
from scipy.stats import multivariate_normal

from aare import Interpolator, calculate_eta2
from aare._aare import ClusterVector_Cluster2x2d, Cluster2x2d, Cluster3x3d, ClusterVector_Cluster3x3d

from conftest import test_data_path

pixel_width = 1e-4
values = np.arange(0.5*pixel_width, 0.1, pixel_width)
num_pixels = values.size
X, Y = np.meshgrid(values, values)
data_points = np.stack([X.ravel(), Y.ravel()], axis=1)
variance = 10*pixel_width
covariance_matrix = np.array([[variance, 0],[0, variance]])



def create_photon_hit_with_gaussian_distribution(mean, covariance_matrix, data_points):
    gaussian = multivariate_normal(mean=mean, cov=covariance_matrix)
    probability_values = gaussian.pdf(data_points)
    return (probability_values.reshape(X.shape)).round() #python bindings only support frame types of uint16_t


def create_2x2cluster_from_frame(frame, pixels_per_superpixel):
    return Cluster2x2d(1, 1, np.array([frame[0:pixels_per_superpixel, 0:pixels_per_superpixel].sum(), 
                                            frame[0:pixels_per_superpixel, pixels_per_superpixel:2*pixels_per_superpixel].sum(), 
                                            frame[pixels_per_superpixel:2*pixels_per_superpixel, 0:pixels_per_superpixel].sum(), 
                                            frame[pixels_per_superpixel:2*pixels_per_superpixel, pixels_per_superpixel:2*pixels_per_superpixel].sum()], dtype=np.float64))


def create_3x3cluster_from_frame(frame, pixels_per_superpixel):
    return Cluster3x3d(1, 1, np.array([frame[0:pixels_per_superpixel, 0:pixels_per_superpixel].sum(), 
                                    frame[0:pixels_per_superpixel, pixels_per_superpixel:2*pixels_per_superpixel].sum(), 
                                    frame[0:pixels_per_superpixel, 2*pixels_per_superpixel:3*pixels_per_superpixel].sum(), 
                                    frame[pixels_per_superpixel:2*pixels_per_superpixel, 0:pixels_per_superpixel].sum(), 
                                    frame[pixels_per_superpixel:2*pixels_per_superpixel, pixels_per_superpixel:2*pixels_per_superpixel].sum(), 
                                    frame[pixels_per_superpixel:2*pixels_per_superpixel, 2*pixels_per_superpixel:3*pixels_per_superpixel].sum(), 
                                    frame[2*pixels_per_superpixel:3*pixels_per_superpixel, 0:pixels_per_superpixel].sum(), 
                                    frame[2*pixels_per_superpixel:3*pixels_per_superpixel, pixels_per_superpixel:2*pixels_per_superpixel].sum(), 
                                    frame[2*pixels_per_superpixel:3*pixels_per_superpixel, 2*pixels_per_superpixel:3*pixels_per_superpixel].sum()], dtype=np.float64))


def calculate_eta_distribution(num_frames, pixels_per_superpixel, random_number_generator, bin_edges_x = bh.axis.Regular(100, -0.2, 1.2), bin_edges_y = bh.axis.Regular(100, -0.2, 1.2), cluster_2x2 = True): 
    hist = bh.Histogram(
    bin_edges_x, 
    bin_edges_y, bh.axis.Regular(1, 0, num_pixels*num_pixels*1/(variance*2*np.pi))) 

    for _ in range(0, num_frames):
        mean_x = random_number_generator.uniform(pixels_per_superpixel*pixel_width, 2*pixels_per_superpixel*pixel_width)
        mean_y = random_number_generator.uniform(pixels_per_superpixel*pixel_width, 2*pixels_per_superpixel*pixel_width)
        frame = create_photon_hit_with_gaussian_distribution(np.array([mean_x, mean_y]), variance, data_points)

        cluster = None

        if cluster_2x2:
            cluster = create_2x2cluster_from_frame(frame, pixels_per_superpixel)
        else: 
            cluster = create_3x3cluster_from_frame(frame, pixels_per_superpixel)

        eta2 = calculate_eta2(cluster)
        hist.fill(eta2[0], eta2[1], eta2[2])

    return hist

@pytest.mark.withdata
def test_interpolation_of_2x2_cluster(test_data_path): 
    """Test Interpolation of 2x2 cluster from Photon hit with Gaussian Distribution""" 

    #TODO maybe better to compute in test instead of loading - depends on eta 
    """ 
    filename = test_data_path/"eta_distributions"/"eta_distribution_2x2cluster_gaussian.pkl"
    with open(filename, "rb") as f:
        eta_distribution = pickle.load(f)
    """

    num_frames = 1000
    pixels_per_superpixel = int(num_pixels*0.5)
    random_number_generator = np.random.default_rng(42)

    eta_distribution = calculate_eta_distribution(num_frames, pixels_per_superpixel, random_number_generator, bin_edges_x = bh.axis.Regular(100, -0.1, 0.6), bin_edges_y = bh.axis.Regular(100, -0.1, 0.6))

    interpolation = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges[:-1])

    #actual photon hit
    mean = 1.2*pixels_per_superpixel*pixel_width
    mean = np.array([mean, mean])
    frame = create_photon_hit_with_gaussian_distribution(mean, covariance_matrix, data_points)
    cluster = create_2x2cluster_from_frame(frame, pixels_per_superpixel)

    clustervec = ClusterVector_Cluster2x2d()
    clustervec.push_back(cluster)

    interpolated_photon = interpolation.interpolate(clustervec)

    assert interpolated_photon.size == 1

    cluster_center = 1.5*pixels_per_superpixel*pixel_width

    scaled_photon_hit = (interpolated_photon[0][0]*pixels_per_superpixel*pixel_width, interpolated_photon[0][1]*pixels_per_superpixel*pixel_width) 

    assert (np.linalg.norm(scaled_photon_hit - mean) < np.linalg.norm(np.array([cluster_center, cluster_center] - mean)))


@pytest.mark.withdata
def test_interpolation_of_3x3_cluster(test_data_path): 
    """Test Interpolation of 3x3 Cluster from Photon hit with Gaussian Distribution""" 

    #TODO maybe better to compute in test instead of loading - depends on eta 
    """ 
    filename = test_data_path/"eta_distributions"/"eta_distribution_3x3cluster_gaussian.pkl"
    with open(filename, "rb") as f:
        eta_distribution = pickle.load(f)
    """

    num_frames = 1000
    pixels_per_superpixel = int(num_pixels/3)
    random_number_generator = np.random.default_rng(42)
    eta_distribution = calculate_eta_distribution(num_frames, pixels_per_superpixel, random_number_generator, bin_edges_x = bh.axis.Regular(100, -0.1, 1.1), bin_edges_y = bh.axis.Regular(100, -0.1, 1.1), cluster_2x2 = False)

    interpolation = Interpolator(eta_distribution, eta_distribution.axes[0].edges, eta_distribution.axes[1].edges, eta_distribution.axes[2].edges[:-1])

    #actual photon hit
    mean_x = (1 + 0.8)*pixels_per_superpixel*pixel_width
    mean_y = (1 + 0.2)*pixels_per_superpixel*pixel_width
    mean = np.array([mean_x, mean_y])
    frame = create_photon_hit_with_gaussian_distribution(mean, covariance_matrix, data_points)
    cluster = create_3x3cluster_from_frame(frame, pixels_per_superpixel)

    clustervec = ClusterVector_Cluster3x3d()
    clustervec.push_back(cluster)

    interpolated_photon = interpolation.interpolate(clustervec)

    assert interpolated_photon.size == 1

    cluster_center = 1.5*pixels_per_superpixel*pixel_width

    scaled_photon_hit = (interpolated_photon[0][0]*pixels_per_superpixel*pixel_width, interpolated_photon[0][1]*pixels_per_superpixel*pixel_width)

    assert (np.linalg.norm(scaled_photon_hit - mean) < np.linalg.norm(np.array([cluster_center, cluster_center] - mean)))

