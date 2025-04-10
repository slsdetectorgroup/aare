#include "aare/ClusterFile.hpp"

#include <algorithm>

namespace aare {

ClusterFile::ClusterFile(const std::filesystem::path &fname, size_t chunk_size,
                         const std::string &mode)
    : m_chunk_size(chunk_size), m_mode(mode) {

    if (mode == "r") {
        fp = fopen(fname.c_str(), "rb");
        if (!fp) {
            throw std::runtime_error("Could not open file for reading: " +
                                     fname.string());
        }
    } else if (mode == "w") {
        fp = fopen(fname.c_str(), "wb");
        if (!fp) {
            throw std::runtime_error("Could not open file for writing: " +
                                     fname.string());
        }
    } else if (mode == "a") {
        fp = fopen(fname.c_str(), "ab");
        if (!fp) {
            throw std::runtime_error("Could not open file for appending: " +
                                     fname.string());
        }
    } else {
        throw std::runtime_error("Unsupported mode: " + mode);
    }
}

void ClusterFile::set_roi(ROI roi){
    m_roi = roi;
}

void ClusterFile::set_noise_map(const NDView<int32_t, 2> noise_map){
    m_noise_map = NDArray<int32_t, 2>(noise_map);
}

void ClusterFile::set_gain_map(const NDView<double, 2> gain_map){
    m_gain_map = NDArray<double, 2>(gain_map);
}

ClusterFile::~ClusterFile() { close(); }

void ClusterFile::close() {
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

void ClusterFile::write_frame(const ClusterVector<int32_t> &clusters) {
    if (m_mode != "w" && m_mode != "a") {
        throw std::runtime_error("File not opened for writing");
    }
    if (!(clusters.cluster_size_x() == 3) &&
        !(clusters.cluster_size_y() == 3)) {
        throw std::runtime_error("Only 3x3 clusters are supported");
    }
    //First write the frame number - 4 bytes
    int32_t frame_number = clusters.frame_number();
    if(fwrite(&frame_number, sizeof(frame_number), 1, fp)!=1){
        throw std::runtime_error(LOCATION + "Could not write frame number");
    }

    //Then write the number of clusters - 4 bytes
    uint32_t n_clusters = clusters.size();
    if(fwrite(&n_clusters, sizeof(n_clusters), 1, fp)!=1){
        throw std::runtime_error(LOCATION + "Could not write number of clusters");
    }

    //Now write the clusters in the frame
    if(fwrite(clusters.data(), clusters.item_size(), clusters.size(), fp)!=clusters.size()){
        throw std::runtime_error(LOCATION + "Could not write clusters");
    }
}


ClusterVector<int32_t> ClusterFile::read_clusters(size_t n_clusters){
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    if (m_noise_map || m_roi){
        return read_clusters_with_cut(n_clusters);
    }else{
        return read_clusters_without_cut(n_clusters);
    }
}

ClusterVector<int32_t> ClusterFile::read_clusters_without_cut(size_t n_clusters) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    
    ClusterVector<int32_t> clusters(3,3, n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    // auto buf = reinterpret_cast<Cluster3x3 *>(clusters.data());
    auto buf = clusters.data();
    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        nph_read += fread((buf + nph_read*clusters.item_size()),
                          clusters.item_size(), nn, fp);
        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, fp)) {
            clusters.set_frame_number(iframe);
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, fp)) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                nph_read += fread((buf + nph_read*clusters.item_size()),
                                  clusters.item_size(), nn, fp);
                m_num_left = nph - nn;
            }
            if (nph_read >= n_clusters)
                break;
        }
    }

    // Resize the vector to the number of clusters.
    // No new allocation, only change bounds.
    clusters.resize(nph_read);
    if(m_gain_map)
        clusters.apply_gain_map(m_gain_map->view());
    return clusters;
}


ClusterVector<int32_t> ClusterFile::read_clusters_with_cut(size_t n_clusters) {
    ClusterVector<int32_t> clusters(3,3);
    clusters.reserve(n_clusters);

    // if there are photons left from previous frame read them first
    if (m_num_left) {
        while(m_num_left && clusters.size() < n_clusters){
            Cluster3x3 c = read_one_cluster();
            if(is_selected(c)){
                clusters.push_back(c.x, c.y, reinterpret_cast<std::byte*>(c.data));
            }
        }
    }

    // we did not have enough clusters left in the previous frame
    // keep on reading frames until reaching n_clusters
    if (clusters.size() < n_clusters) {
        // sanity check
        if (m_num_left) {
            throw std::runtime_error(LOCATION + "Entered second loop with clusters left\n");
        }
        
        int32_t frame_number = 0; // frame number needs to be 4 bytes!
        while (fread(&frame_number, sizeof(frame_number), 1, fp)) {
            if (fread(&m_num_left, sizeof(m_num_left), 1, fp)) {
                clusters.set_frame_number(frame_number); //cluster vector will hold the last frame number
                while(m_num_left && clusters.size() < n_clusters){
                    Cluster3x3 c = read_one_cluster();
                    if(is_selected(c)){
                        clusters.push_back(c.x, c.y, reinterpret_cast<std::byte*>(c.data));
                    }
                }
            }

            // we have enough clusters, break out of the outer while loop
            if (clusters.size() >= n_clusters)
                break;
        }

    }
    if(m_gain_map)
        clusters.apply_gain_map(m_gain_map->view());

    return clusters;
}

Cluster3x3 ClusterFile::read_one_cluster(){
    Cluster3x3 c;
    auto rc = fread(&c, sizeof(c), 1, fp);
    if (rc != 1) {
        throw std::runtime_error(LOCATION + "Could not read cluster");
    }
    --m_num_left;
    return c;
}

ClusterVector<int32_t> ClusterFile::read_frame(){
    if (m_mode != "r") {
        throw std::runtime_error(LOCATION + "File not opened for reading");
    }
    if (m_noise_map || m_roi){
        return read_frame_with_cut();
    }else{
        return read_frame_without_cut();
    }
}

ClusterVector<int32_t> ClusterFile::read_frame_without_cut() {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    if (m_num_left) {
        throw std::runtime_error(
            "There are still photons left in the last frame");
    }
    int32_t frame_number;
    if (fread(&frame_number, sizeof(frame_number), 1, fp) != 1) {
        throw std::runtime_error(LOCATION + "Could not read frame number");
    }

    int32_t n_clusters; // Saved as 32bit integer in the cluster file
    if (fread(&n_clusters, sizeof(n_clusters), 1, fp) != 1) {
        throw std::runtime_error(LOCATION + "Could not read number of clusters");
    }

    ClusterVector<int32_t> clusters(3, 3, n_clusters);
    clusters.set_frame_number(frame_number);

    if (fread(clusters.data(), clusters.item_size(), n_clusters, fp) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error(LOCATION + "Could not read clusters");
    }
    clusters.resize(n_clusters);
    if (m_gain_map)
        clusters.apply_gain_map(m_gain_map->view());
    return clusters;
}

ClusterVector<int32_t> ClusterFile::read_frame_with_cut() {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }
    if (m_num_left) {
        throw std::runtime_error(
            "There are still photons left in the last frame");
    }
    int32_t frame_number;
    if (fread(&frame_number, sizeof(frame_number), 1, fp) != 1) {
        throw std::runtime_error("Could not read frame number");
    }


    if (fread(&m_num_left, sizeof(m_num_left), 1, fp) != 1) {
        throw std::runtime_error("Could not read number of clusters");
    }
    
    ClusterVector<int32_t> clusters(3, 3);
    clusters.reserve(m_num_left);
    clusters.set_frame_number(frame_number);
    while(m_num_left){
        Cluster3x3 c = read_one_cluster();
        if(is_selected(c)){
            clusters.push_back(c.x, c.y, reinterpret_cast<std::byte*>(c.data));
        }
    }
    if (m_gain_map)
        clusters.apply_gain_map(m_gain_map->view());
    return clusters;
}



bool ClusterFile::is_selected(Cluster3x3 &cl) {
    //Should fail fast
    if (m_roi) {
        if (!(m_roi->contains(cl.x, cl.y))) {
            return false;
        }
    }
    if (m_noise_map){
        int32_t sum_1x1 = cl.data[4]; // central pixel
        int32_t sum_2x2 = cl.sum_2x2(); // highest sum of 2x2 subclusters
        int32_t sum_3x3 = cl.sum(); // sum of all pixels

        auto noise = (*m_noise_map)(cl.y, cl.x); //TODO! check if this is correct
        if (sum_1x1 <= noise || sum_2x2 <= 2 * noise || sum_3x3 <= 3 * noise) {
            return false;
        }
    }
    //we passed all checks
    return true;
}

NDArray<double, 2> calculate_eta2(ClusterVector<int> &clusters) {
    //TOTO! make work with 2x2 clusters
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});
    
    if (clusters.cluster_size_x() == 3 || clusters.cluster_size_y() == 3) {
        for (size_t i = 0; i < clusters.size(); i++) {
            auto e = calculate_eta2(clusters.at<Cluster3x3>(i));
            eta2(i, 0) = e.x;
            eta2(i, 1) = e.y;
        }
    }else if(clusters.cluster_size_x() == 2 || clusters.cluster_size_y() == 2){
        for (size_t i = 0; i < clusters.size(); i++) {
            auto e = calculate_eta2(clusters.at<Cluster2x2>(i));
            eta2(i, 0) = e.x;
            eta2(i, 1) = e.y;
        }
    }else{
        throw std::runtime_error("Only 3x3 and 2x2 clusters are supported");
    }
    
    return eta2;
}

/** 
 * @brief Calculate the eta2 values for a 3x3 cluster and return them in a Eta2 struct
 * containing etay, etax and the corner of the cluster. 
*/
Eta2 calculate_eta2(Cluster3x3 &cl) {
    Eta2 eta{};

    std::array<int32_t, 4> tot2;
    tot2[0] = cl.data[0] + cl.data[1] + cl.data[3] + cl.data[4];
    tot2[1] = cl.data[1] + cl.data[2] + cl.data[4] + cl.data[5];
    tot2[2] = cl.data[3] + cl.data[4] + cl.data[6] + cl.data[7];
    tot2[3] = cl.data[4] + cl.data[5] + cl.data[7] + cl.data[8];

    auto c = std::max_element(tot2.begin(), tot2.end()) - tot2.begin();
    eta.sum = tot2[c];
    switch (c) {
    case cBottomLeft:
        if ((cl.data[3] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomLeft;
        break;
    case cBottomRight:
        if ((cl.data[2] + cl.data[5]) != 0)
            eta.x =
                static_cast<double>(cl.data[5]) / (cl.data[4] + cl.data[5]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomRight;
        break;
    case cTopLeft:
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopLeft;
        break;
    case cTopRight:
        if ((cl.data[5] + cl.data[4]) != 0)
            eta.x =
                static_cast<double>(cl.data[5]) / (cl.data[5] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y =
                static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopRight;
        break;
    // no default to allow compiler to warn about missing cases
    }
    return eta;
}


Eta2 calculate_eta2(Cluster2x2 &cl) {
    Eta2 eta{};
    if ((cl.data[0] + cl.data[1]) != 0)
        eta.x = static_cast<double>(cl.data[1]) / (cl.data[0] + cl.data[1]);
    if ((cl.data[0] + cl.data[2]) != 0)
        eta.y = static_cast<double>(cl.data[2]) / (cl.data[0] + cl.data[2]);
    eta.sum = cl.data[0] + cl.data[1] + cl.data[2]+ cl.data[3];
    eta.c = cBottomLeft; //TODO! This is not correct, but need to put something
    return eta;
}


} // namespace aare