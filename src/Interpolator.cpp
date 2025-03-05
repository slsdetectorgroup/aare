#include "aare/Interpolator.hpp"

namespace aare {

Interpolator::Interpolator(NDView<double, 3> etacube, NDView<double, 1> xbins,
                           NDView<double, 1> ybins, NDView<double, 1> ebins)
    : m_ietax(etacube), m_ietay(etacube), m_etabinsx(xbins), m_etabinsy(ybins), m_energy_bins(ebins) {
    if (etacube.shape(0) != xbins.size() || etacube.shape(1) != ybins.size() ||
        etacube.shape(2) != ebins.size()) {
        throw std::invalid_argument(
            "The shape of the etacube does not match the shape of the bins");
    }

    // Cumulative sum in the x direction, can maybe be combined with a copy?
    for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            for (ssize_t i = 1; i < m_ietax.shape(0); i++) {
                m_ietax(i, j, k) += m_ietax(i - 1, j, k);
            }
        }
    }

    // Normalize by the highest row, if norm less than 1 don't do anything
    for (ssize_t k = 0; k < m_ietax.shape(2); k++) {
        for (ssize_t j = 0; j < m_ietax.shape(1); j++) {
            auto val = m_ietax(m_ietax.shape(0) - 1, j, k);
            double norm = val < 1 ? 1 : val;
            for (ssize_t i = 0; i < m_ietax.shape(0); i++) {
                m_ietax(i, j, k) /= norm;
            }
        }
    }

    // Cumulative sum in the y direction
    for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
        for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
            for (ssize_t j = 1; j < m_ietay.shape(1); j++) {
                m_ietay(i, j, k) += m_ietay(i, j - 1, k);
            }
        }
    }

    // Normalize by the highest column, if norm less than 1 don't do anything
    for (ssize_t k = 0; k < m_ietay.shape(2); k++) {
        for (ssize_t i = 0; i < m_ietay.shape(0); i++) {
            auto val = m_ietay(i, m_ietay.shape(1) - 1, k);
            double norm = val < 1 ? 1 : val;
            for (ssize_t j = 0; j < m_ietay.shape(1); j++) {
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
    
            //Now do some actual interpolation. 
            //Find which energy bin the cluster is in
            //TODO! Could we use boost-histogram Axis.index here? 
            ssize_t idx = std::lower_bound(m_energy_bins.begin(), m_energy_bins.end(), photon.energy)-m_energy_bins.begin();
            auto ix = std::lower_bound(m_etabinsx.begin(), m_etabinsx.end(), eta.x)- m_etabinsx.begin();
            auto iy = std::lower_bound(m_etabinsy.begin(), m_etabinsy.end(), eta.y)- m_etabinsy.begin();
    
            
            // ibx=(np.abs(etabinsx - ex)).argmin() #Find out which bin the eta should land in 
            // iby=(np.abs(etabinsy - ey)).argmin()
            double dX, dY;
            int ex, ey;
            // cBottomLeft = 0,
            // cBottomRight = 1,
            // cTopLeft = 2,
            // cTopRight = 3
            switch (eta.c) {
            case cTopLeft:
                dX = -1.;
                dY = 0;
                break;
            case cTopRight:;
                dX = 0;
                dY = 0;
                break;
            case cBottomLeft:
                dX = -1.;
                dY = -1.;
                break;
            case cBottomRight:
                dX = 0;
                dY = -1.;
                break;
            }
            photon.x += m_ietax(ix, iy, idx) + dX + 0.5;
            photon.y += m_ietay(ix, iy, idx) + dY + 0.5;
    
    
            // fmt::print("x: {}, y: {}, energy: {}\n", photon.x, photon.y, photon.energy);
            photons.push_back(photon);
        }
    }else if(clusters.cluster_size_x() == 2 || clusters.cluster_size_y() == 2){
        //TODO! Implement 2x2 interpolation
        for (size_t i = 0; i<clusters.size(); i++){

            auto cluster = clusters.at<Cluster2x2>(i);
            Eta2 eta= calculate_eta2(cluster);
    
            Photon photon;
            photon.x = cluster.x;
            photon.y = cluster.y;
            photon.energy = eta.sum;
    
            //Now do some actual interpolation. 
            //Find which energy bin the cluster is in
            //TODO! Could we use boost-histogram Axis.index here? 
            ssize_t idx = std::lower_bound(m_energy_bins.begin(), m_energy_bins.end(), photon.energy)-m_energy_bins.begin();
            // auto ix = std::lower_bound(m_etabinsx.begin(), m_etabinsx.end(), eta.x)- m_etabinsx.begin();
            // auto iy = std::lower_bound(m_etabinsy.begin(), m_etabinsy.end(), eta.y)- m_etabinsy.begin();
            // if(ix<0) ix=0;
            // if(iy<0) iy=0;

            auto find_index = [](NDArray<double, 1>& etabins, double val){
                auto iter = std::min_element(etabins.begin(), etabins.end(),
                [val,etabins](double a, double b) {
                    return std::abs(a - val) < std::abs(b - val);
                });
                return std::distance(etabins.begin(), iter);
            };
            auto ix = find_index(m_etabinsx, eta.x)-1;
            auto iy = find_index(m_etabinsy, eta.y)-1;    
            if(ix<0) ix=0;
            if(iy<0) iy=0;

            photon.x += m_ietax(ix, iy, 0)*2; //eta goes between 0 and 1 but we could move the hit anywhere in the 2x2
            photon.y += m_ietay(ix, iy, 0)*2;

            // photon.x = ix;
            // photon.y = idx;
            photons.push_back(photon);
        }
        
    }else{
        throw std::runtime_error("Only 3x3 and 2x2 clusters are supported for interpolation");
    }
  

    return photons;
}

} // namespace aare