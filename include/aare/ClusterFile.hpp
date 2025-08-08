#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/GainMap.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include "aare/logger.hpp"

#include <filesystem>
#include <fstream>
#include <optional>

namespace aare {

/*
Binary cluster file. Expects data to be laid out as:
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
 *       int16_t x, int16_t y, int32_t data[9] * number_of_clusters
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       etc.
 */
template <typename ClusterType,
          typename Enable = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterFile {
    FILE *fp{};
    const std::string m_filename{};
    uint32_t m_num_left{};    /*Number of photons left in frame*/
    size_t m_chunk_size{};    /*Number of clusters to read at a time*/
    std::string m_mode;       /*Mode to open the file in*/
    std::optional<ROI> m_roi; /*Region of interest, will be applied if set*/
    std::optional<NDArray<int32_t, 2>>
        m_noise_map; /*Noise map to cut photons, will be applied if set*/
    std::optional<InvertedGainMap> m_gain_map; /*Gain map to apply to the
                                          clusters, will be applied if set*/

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
                const std::string &mode = "r")

        : m_filename(fname.string()), m_chunk_size(chunk_size), m_mode(mode) {

        if (mode == "r") {
            fp = fopen(m_filename.c_str(), "rb");
            if (!fp) {
                throw std::runtime_error("Could not open file for reading: " +
                                         m_filename);
            }
        } else if (mode == "w") {
            fp = fopen(m_filename.c_str(), "wb");
            if (!fp) {
                throw std::runtime_error("Could not open file for writing: " +
                                         m_filename);
            }
        } else if (mode == "a") {
            fp = fopen(m_filename.c_str(), "ab");
            if (!fp) {
                throw std::runtime_error("Could not open file for appending: " +
                                         m_filename);
            }
        } else {
            throw std::runtime_error("Unsupported mode: " + mode);
        }
    }

    ~ClusterFile() { close(); }

    /**
     * @brief Read n_clusters clusters from the file discarding
     * frame numbers. If EOF is reached the returned vector will
     * have less than n_clusters clusters
     */
    ClusterVector<ClusterType> read_clusters(size_t n_clusters) {
        if (m_mode != "r") {
            throw std::runtime_error("File not opened for reading");
        }
        if (m_noise_map || m_roi) {
            return read_clusters_with_cut(n_clusters);
        } else {
            return read_clusters_without_cut(n_clusters);
        }
    }

    /**
     * @brief Read a single frame from the file and return the
     * clusters. The cluster vector will have the frame number
     * set.
     * @throws std::runtime_error if the file is not opened for
     * reading or the file pointer not at the beginning of a
     * frame
     */
    ClusterVector<ClusterType> read_frame() {
        if (m_mode != "r") {
            throw std::runtime_error(LOCATION + "File not opened for reading");
        }
        if (m_noise_map || m_roi) {
            return read_frame_with_cut();
        } else {
            return read_frame_without_cut();
        }
    }

    void write_frame(const ClusterVector<ClusterType> &clusters) {
        if (m_mode != "w" && m_mode != "a") {
            throw std::runtime_error("File not opened for writing");
        }

        int32_t frame_number = clusters.frame_number();
        fwrite(&frame_number, sizeof(frame_number), 1, fp);
        uint32_t n_clusters = clusters.size();
        fwrite(&n_clusters, sizeof(n_clusters), 1, fp);
        fwrite(clusters.data(), clusters.item_size(), clusters.size(), fp);
    }

    /**
     * @brief Return the chunk size
     */
    size_t chunk_size() const { return m_chunk_size; }

    /**
     * @brief Set the region of interest to use when reading
     * clusters. If set only clusters within the ROI will be
     * read.
     */
    void set_roi(ROI roi) { m_roi = roi; }

    /**
     * @brief Set the noise map to use when reading clusters. If
     * set clusters below the noise level will be discarded.
     * Selection criteria one of: Central pixel above noise,
     * highest 2x2 sum above 2 * noise, total sum above 3 *
     * noise.
     */
    void set_noise_map(const NDView<int32_t, 2> noise_map) {
        m_noise_map = NDArray<int32_t, 2>(noise_map);
    }

    /**
     * @brief Set the gain map to use when reading clusters. If set the gain map
     * will be applied to the clusters that pass ROI and noise_map selection.
     * The gain map is expected to be in ADU/energy.
     */
    void set_gain_map(const NDView<double, 2> gain_map) {
        m_gain_map = InvertedGainMap(gain_map);
    }

    void set_gain_map(const InvertedGainMap &gain_map) {
        m_gain_map = gain_map;
    }

    void set_gain_map(const InvertedGainMap &&gain_map) {
        m_gain_map = gain_map;
    }

    /**
     * @brief Close the file. If not closed the file will be
     * closed in the destructor
     */
    void close() {
        if (fp) {
            fclose(fp);
            fp = nullptr;
        }
    }

    /** @brief Open the file in specific mode
     *
     */
    void open(const std::string &mode) {
        if (fp) {
            close();
        }

        if (mode == "r") {
            fp = fopen(m_filename.c_str(), "rb");
            if (!fp) {
                throw std::runtime_error("Could not open file for reading: " +
                                         m_filename);
            }
            m_mode = "r";
        } else if (mode == "w") {
            fp = fopen(m_filename.c_str(), "wb");
            if (!fp) {
                throw std::runtime_error("Could not open file for writing: " +
                                         m_filename);
            }
            m_mode = "w";
        } else if (mode == "a") {
            fp = fopen(m_filename.c_str(), "ab");
            if (!fp) {
                throw std::runtime_error("Could not open file for appending: " +
                                         m_filename);
            }
            m_mode = "a";
        } else {
            throw std::runtime_error("Unsupported mode: " + mode);
        }
    }

  private:
    ClusterVector<ClusterType> read_clusters_with_cut(size_t n_clusters);
    ClusterVector<ClusterType> read_clusters_without_cut(size_t n_clusters);
    ClusterVector<ClusterType> read_frame_with_cut();
    ClusterVector<ClusterType> read_frame_without_cut();
    bool is_selected(ClusterType &cl);
    ClusterType read_one_cluster();
};

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters_without_cut(size_t n_clusters) {
    if (m_mode != "r") {
        throw std::runtime_error("File not opened for reading");
    }

    ClusterVector<ClusterType> clusters(n_clusters);
    clusters.resize(n_clusters);

    int32_t iframe = 0; // frame number needs to be 4 bytes!
    size_t nph_read = 0;
    uint32_t nn = m_num_left;
    uint32_t nph = m_num_left; // number of clusters in frame needs to be 4

    auto buf = clusters.data();
    // if there are photons left from previous frame read them first
    if (nph) {
        if (nph > n_clusters) {
            // if we have more photons left in the frame then photons to
            // read we read directly the requested number
            nn = n_clusters;
        } else {
            nn = nph;
        }
        nph_read += fread((buf + nph_read), clusters.item_size(), nn, fp);
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

                nph_read +=
                    fread((buf + nph_read), clusters.item_size(), nn, fp);
                m_num_left = nph - nn;
            }
            if (nph_read >= n_clusters)
                break;
        }
    }

    // Resize the vector to the number o f clusters.
    // No new allocation, only change bounds.
    clusters.resize(nph_read);
    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_clusters_with_cut(size_t n_clusters) {
    ClusterVector<ClusterType> clusters;
    clusters.reserve(n_clusters);

    // if there are photons left from previous frame read them first
    if (m_num_left) {
        while (m_num_left && clusters.size() < n_clusters) {
            ClusterType c = read_one_cluster();
            if (is_selected(c)) {
                clusters.push_back(c);
            }
        }
    }

    // we did not have enough clusters left in the previous frame
    // keep on reading frames until reaching n_clusters
    if (clusters.size() < n_clusters) {
        // sanity check
        if (m_num_left) {
            throw std::runtime_error(
                LOCATION + "Entered second loop with clusters left\n");
        }

        int32_t frame_number = 0; // frame number needs to be 4 bytes!
        while (fread(&frame_number, sizeof(frame_number), 1, fp)) {
            if (fread(&m_num_left, sizeof(m_num_left), 1, fp)) {
                clusters.set_frame_number(
                    frame_number); // cluster vector will hold the last
                                   // frame number
                while (m_num_left && clusters.size() < n_clusters) {
                    ClusterType c = read_one_cluster();
                    if (is_selected(c)) {
                        clusters.push_back(c);
                    }
                }
            }

            // we have enough clusters, break out of the outer while loop
            if (clusters.size() >= n_clusters)
                break;
        }
    }
    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);

    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterType ClusterFile<ClusterType, Enable>::read_one_cluster() {
    ClusterType c;
    auto rc = fread(&c, sizeof(c), 1, fp);
    if (rc != 1) {
        throw std::runtime_error(LOCATION + "Could not read cluster");
    }
    --m_num_left;
    return c;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_frame_without_cut() {
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
        throw std::runtime_error(LOCATION +
                                 "Could not read number of clusters");
    }

    LOG(logDEBUG1) << "Reading " << n_clusters << " clusters from frame "
                   << frame_number;

    ClusterVector<ClusterType> clusters(n_clusters);
    clusters.set_frame_number(frame_number);
    clusters.resize(n_clusters);

    LOG(logDEBUG1) << "clusters.item_size(): " << clusters.item_size();

    if (fread(clusters.data(), clusters.item_size(), n_clusters, fp) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error(LOCATION + "Could not read clusters");
    }

    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);
    return clusters;
}

template <typename ClusterType, typename Enable>
ClusterVector<ClusterType>
ClusterFile<ClusterType, Enable>::read_frame_with_cut() {
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

    ClusterVector<ClusterType> clusters;
    clusters.reserve(m_num_left);
    clusters.set_frame_number(frame_number);
    while (m_num_left) {
        ClusterType c = read_one_cluster();
        if (is_selected(c)) {
            clusters.push_back(c);
        }
    }
    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);
    return clusters;
}

template <typename ClusterType, typename Enable>
bool ClusterFile<ClusterType, Enable>::is_selected(ClusterType &cl) {
    // Should fail fast
    if (m_roi) {
        if (!(m_roi->contains(cl.x, cl.y))) {
            return false;
        }
    }

    size_t cluster_center_index =
        (ClusterType::cluster_size_x / 2) +
        (ClusterType::cluster_size_y / 2) * ClusterType::cluster_size_x;

    if (m_noise_map) {
        auto sum_1x1 = cl.data[cluster_center_index]; // central pixel
        auto sum_2x2 = cl.max_sum_2x2().first; // highest sum of 2x2 subclusters
        auto total_sum = cl.sum();             // sum of all pixels

        auto noise =
            (*m_noise_map)(cl.y, cl.x); // TODO! check if this is correct
        if (sum_1x1 <= noise || sum_2x2 <= 2 * noise ||
            total_sum <= 3 * noise) {
            return false;
        }
    }
    // we passed all checks
    return true;
}

} // namespace aare
