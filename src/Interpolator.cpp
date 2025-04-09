#include "aare/Interpolator.hpp"
#include "aare/algorithm.hpp"

namespace aare {

Interpolator::Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                           NDView<double, 1> ybins, NDView<double, 1> ebins)
    : m_ietax(etacube), m_ietay(etacube), m_etabinsx(xbins), m_etabinsy(ybins), m_energy_bins(ebins) {
    if (etacube.shape(0) != xbins.size() || etacube.shape(1) != ybins.size() ||
        etacube.shape(2) != ebins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }

    // Cumulative sum in the x direction
    for (ssize_t i = 1; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                m_ietax(i, j, k) += m_ietax(i - 1, j, k);
            }
        }
    }

    // Normalize by the highest row, if norm less than 1 don't do anything
    for (ssize_t i = 0; i < m_ietax.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
                auto val = m_ietax(m_ietax.shape(0) - 1, j, k);
                double norm = val < 1 ? 1 : val;
                m_ietax(i, j, k) /= norm;
            }
        }
    }

    // Cumulative sum in the y direction
    for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
        for (ssize_t j = 1; j < m_ietay.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
                m_ietay(i, j, k) += m_ietay(i, j - 1, k);
            }
        }
    }

    // Normalize by the highest column, if norm less than 1 don't do anything
    for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
        for (ssize_t j = 0; j < m_ietay.shape(1); j++) {
            for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
                auto val = m_ietay(i, m_ietay.shape(1) - 1, k);
                double norm = val < 1 ? 1 : val;
                m_ietay(i, j, k) /= norm;
            }
        }
    }
}

std::vector<Photon> Interpolator::interpolate(const ClusterVector<int32_t>& clusters) {
    std::vector<Photon> photons;
    photons.reserve(clusters.size());

    if (clusters.cluster_size_x() == 3 || clusters.cluster_size_y() == 3) {
        for (size_t i = 0; i<clusters.size(); i++){

            auto cluster = clusters.at<Cluster3x3>(i);
            Eta2 eta= calculate_eta2(cluster);
    
            Photon photon;
            photon.x = cluster.x;
            photon.y = cluster.y;
            photon.energy = eta.sum;
    

            //Finding the index of the last element that is smaller
            //should work fine as long as we have many bins
            auto ie = last_smaller(m_energy_bins, photon.energy);
            auto ix = last_smaller(m_etabinsx, eta.x);
            auto iy = last_smaller(m_etabinsy, eta.y); 
            
            double dX{}, dY{};
            // cBottomLeft = 0,
            // cBottomRight = 1,
            // cTopLeft = 2,
            // cTopRight = 3
            switch (eta.c) {
            case cTopLeft:
                dX = -1.;
                dY = 0.;
                break;
            case cTopRight:;
                dX = 0.;
                dY = 0.;
                break;
            case cBottomLeft:
                dX = -1.;
                dY = -1.;
                break;
            case cBottomRight:
                dX = 0.; 
                dY = -1.;
                break;
            }
            photon.x += m_ietax(ix, iy, ie)*2 + dX;
            photon.y += m_ietay(ix, iy, ie)*2 + dY;
            photons.push_back(photon);
        }
    }else if(clusters.cluster_size_x() == 2 || clusters.cluster_size_y() == 2){
        for (size_t i = 0; i<clusters.size(); i++){
            auto cluster = clusters.at<Cluster2x2>(i);
            Eta2 eta= calculate_eta2(cluster);
    
            Photon photon;
            photon.x = cluster.x;
            photon.y = cluster.y;
            photon.energy = eta.sum;
    
            //Now do some actual interpolation. 
            //Find which energy bin the cluster is in
            // auto ie = nearest_index(m_energy_bins, photon.energy)-1;
            // auto ix = nearest_index(m_etabinsx, eta.x)-1;
            // auto iy = nearest_index(m_etabinsy, eta.y)-1;  
            //Finding the index of the last element that is smaller
            //should work fine as long as we have many bins
            auto ie = last_smaller(m_energy_bins, photon.energy);
            auto ix = last_smaller(m_etabinsx, eta.x);
            auto iy = last_smaller(m_etabinsy, eta.y); 

            photon.x += m_ietax(ix, iy, ie)*2; //eta goes between 0 and 1 but we could move the hit anywhere in the 2x2
            photon.y += m_ietay(ix, iy, ie)*2;
            photons.push_back(photon);
        }
        
    }else{
        throw std::runtime_error("Only 3x3 and 2x2 clusters are supported for interpolation");
    }
  

    return photons;
}

} // namespace aare