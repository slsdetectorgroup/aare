#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fstream>

namespace aare {

/*
Binary cluster file. Expects data to be layed out as:
int32_t frame_number
uint32_t number_of_clusters
int16_t x, int16_t y, int32_t data[9] x number_of_clusters
int32_t frame_number
uint32_t number_of_clusters
....
*/

// TODO: change to support any type of clusters, e.g. header line with
// clsuter_size_x, cluster_size_y,
/**
 * @brief Class to read and write cluster files
 * Expects data to be laid out as:
 *
 *
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       int16_t x, int16_t y, int32_t data[9] x number_of_clusters
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       etc.
 */
template <typename ClusterType,
          typename Enable = std::enable_if_t<is_cluster_v<ClusterType>, bool>>
class ClusterFile {
    FILE *fp{};
    uint32_t m_num_left{};
    size_t m_chunk_size{};
    const std::string m_mode;

  public:
    /**
     * @brief Construct a new Cluster File object
     * @param fname path to the file
     * @param chunk_size number of clusters to read at a time when iterating
     * over the file
     * @param mode mode to open the file in. "r" for reading, "w" for writing,
     * "a" for appending
     * @throws std::runtime_error if the file could not be opened
     */
    ClusterFile(const std::filesystem::path &fname, size_t chunk_size = 1000,
                const std::string &mode = "r");

    ~ClusterFile();

    /**
     * @brief Read n_clusters clusters from the file discarding frame numbers.
     * If EOF is reached the returned vector will have less than n_clusters
     * clusters
     */
    ClusterVector<ClusterType> read_clusters(size_t n_clusters);

    ClusterVector<ClusterType> read_clusters(size_t n_clusters, ROI roi);

    /**
     * @brief Read a single frame from the file and return the clusters. The
     * cluster vector will have the frame number set.
     * @throws std::runtime_error if the file is not opened for reading or the
     * file pointer not at the beginning of a frame
     */
    ClusterVector<ClusterType> read_frame();

    void write_frame(const ClusterVector<ClusterType> &clusters);

    // Need to be migrated to support NDArray and return a ClusterVector
    // std::vector<Cluster3x3>
    // read_cluster_with_cut(size_t n_clusters, double *noise_map, int nx, int
    // ny);

    /**
     * @brief Return the chunk size
     */
    size_t chunk_size() const { return m_chunk_size; }

    /**
     * @brief Close the file. If not closed the file will be closed in the
     * destructor
     */
    void close();
};

template <typename ClusterType, typename Enable>
ClusterFile<ClusterType, Enable>::ClusterFile(
    const std::filesystem::path &fname, size_t chunk_size,
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

template <typename ClusterType, typename Enable>
ClusterFile<ClusterType, Enable>::~ClusterFile() {
    close();
}

template <typename ClusterType, typename Enable>
void ClusterFile<ClusterType, Enable>::close() {
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

// TODO generally supported for all clsuter types
template <typename ClusterType, typename Enable>
void ClusterFile<ClusterType, Enable>::write_frame(
    const ClusterVector<ClusterType> &clusters) {
    if (m_mode != "w" && m_mode != "a") {
        throw std::runtime_error("File not opened for writing");
    }
    if (!(clusters.cluster_size_x() == 3) &&
        !(clusters.cluster_size_y() == 3)) {
        throw std::runtime_error("Only 3x3 clusters are supported");
    }
    int32_t frame_number = clusters.frame_number();
    fwrite(&frame_number, sizeof(frame_number), 1, fp);
    uint32_t n_clusters = clusters.size();
    fwrite(&n_clusters, sizeof(n_clusters), 1, fp);
    fwrite(clusters.data(), clusters.item_size(), clusters.size(), fp);
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters(size_t n_clusters) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }

    ClusterVector<ClusterType> clusters(n_clusters);

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
        nph_read += fread((buf + nph_read * clusters.item_size()),
                          clusters.item_size(), nn, fp);
        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, fp)) {
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, fp)) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                nph_read += fread((buf + nph_read * clusters.item_size()),
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
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters(size_t n_clusters, ROI roi) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }

    ClusterVector<ClusterType> clusters;
    clusters.reserve(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    // auto buf = reinterpret_cast<Cluster3x3 *>(clusters.data());
    // auto buf = clusters.data();

    ClusterType tmp; // this would break if the cluster size changes

    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to read we
            // read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        // Read one cluster, in the ROI push back
        //  nph_read += fread((buf + nph_read*clusters.item_size()),
        //                    clusters.item_size(), nn, fp);
        for (size_t i = 0; i < nn; i++) {
            fread(&tmp, sizeof(tmp), 1, fp);
            if (tmp.x >= roi.xmin && tmp.x <= roi.xmax && tmp.y >= roi.ymin &&
                tmp.y <= roi.ymax) {
                // clusters.push_back(tmp.x, tmp.y,
                // reinterpret_cast<std::byte *>(tmp.data));
                clusters.push_back(tmp);
                nph_read++;
            }
        }

        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, fp)) {
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, fp)) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                // nph_read += fread((buf + nph_read*clusters.item_size()),
                //                   clusters.item_size(), nn, fp);
                for (size_t i = 0; i < nn; i++) {
                    fread(&tmp, sizeof(tmp), 1, fp);
                    if (tmp.x >= roi.xmin && tmp.x <= roi.xmax &&
                        tmp.y >= roi.ymin && tmp.y <= roi.ymax) {
                        // clusters.push_back(
                        // tmp.x, tmp.y,
                        // reinterpret_cast<std::byte *>(tmp.data));
                        clusters.push_back(tmp);
                        nph_read++;
                    }
                }
                m_num_left = nph - nn;
            }
            if (nph_read >= n_clusters)
                break;
        }
    }

    // Resize the vector to the number of clusters.
    // No new allocation, only change bounds.
    clusters.resize(nph_read);
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType> ClusterFile<ClusterType, Enable>::read_frame() {
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

    int32_t n_clusters; // Saved as 32bit integer in the cluster file
    if (fread(&n_clusters, sizeof(n_clusters), 1, fp) != 1) {
        throw std::runtime_error("Could not read number of clusters");
    }
    // std::vector<Cluster3x3> clusters(n_clusters);
    ClusterVector<ClusterType> clusters(n_clusters);
    clusters.set_frame_number(frame_number);

    if (fread(clusters.data(), clusters.item_size(), n_clusters, fp) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error("Could not read clusters");
    }
    clusters.resize(n_clusters);
    return clusters;
}

} // namespace aare
