// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/FilePtr.hpp"
#include "aare/GainMap.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include "aare/logger.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

namespace aare {

/**
 * @brief Read and write legacy binary cluster files.
 *
 * Each frame is stored as:
 *
 *       int32_t frame_number
 *       uint32_t number_of_clusters
 *       ClusterType clusters[number_of_clusters]
 *
 * The format stores clusters as their native in-memory representation and has
 * no metadata describing the cluster dimensions, value type, coordinate type,
 * padding, or byte order. Readers must therefore use the same ClusterType and
 * a compatible platform ABI as the writer.
 */
template <typename ClusterType,
          typename Enable = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterFile {
    FilePtr m_fp;
    std::string m_filename{};
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
     * @brief Open a cluster file.
     * @param fname Path to the file.
     * @param chunk_size Number of clusters returned by each iterator step.
     * @param mode File mode: "r" to read, "w" to truncate and write, or "a"
     * to append.
     * @throws std::runtime_error If the mode is unsupported or the file cannot
     * be opened.
     */
    ClusterFile(const std::filesystem::path &fname, size_t chunk_size = 1000,
                const std::string &mode = "r")

        : m_filename(fname.string()), m_chunk_size(chunk_size) {
        open(mode);
    }

    /**
     * @brief Read up to n_clusters without preserving frame boundaries.
     * @param n_clusters Maximum number of selected clusters to return.
     * @return A cluster vector that may contain fewer clusters at end of file.
     * @note The returned vector may combine data from several frames, so its
     * frame number must not be used as per-cluster metadata.
     * @throws std::runtime_error If the file is not open for reading or a
     * filtered read encounters an incomplete cluster record.
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
     * @brief Read the next complete frame.
     * @return The selected clusters with the stored frame number set, or
     * std::nullopt at a clean end of file before the next frame.
     * @throws std::runtime_error If the file is not open for reading, a prior
     * partial-frame read left clusters unread, or the frame is incomplete.
     * @note A complete frame produces an engaged optional even when it contains
     * no clusters or all of its clusters are removed by the configured filters.
     */
    std::optional<ClusterVector<ClusterType>> read_frame() {
        ClusterVector<ClusterType> clusters(0);
        if (!read_frame(clusters)) {
            return std::nullopt;
        }
        return std::optional<ClusterVector<ClusterType>>{std::move(clusters)};
    }

    /**
     * @brief Read the next complete frame into an existing cluster vector.
     * @param clusters Destination whose storage is reused when large enough.
     * Existing clusters are replaced after a frame header is read and remain
     * unchanged at a clean end of file.
     * @return true when a complete frame was read, or false at a clean end of
     * file before the next frame.
     * @throws std::runtime_error If the file is not open for reading, a prior
     * partial-frame read left clusters unread, or a frame is incomplete.
     * @note A complete frame is a successful read even when it contains no
     * clusters or all of its clusters are removed by the configured filters.
     */
    bool read_frame(ClusterVector<ClusterType> &clusters) {
        if (m_mode != "r") {
            throw std::runtime_error(LOCATION + "File not opened for reading");
        }
        if (m_num_left) {
            throw std::runtime_error(
                LOCATION + "There are still clusters left in the last frame");
        }

        if (m_noise_map || m_roi) {
            return read_frame_with_cut(clusters);
        }
        return read_frame_without_cut(clusters);
    }

    /**
     * @brief Write one frame to the file.
     * @param clusters Clusters to write, including their frame number.
     * @throws std::runtime_error If the file is not open for writing or any
     * part of the frame cannot be written completely.
     */
    void write_frame(const ClusterVector<ClusterType> &clusters) {
        if (m_mode != "w" && m_mode != "a") {
            throw std::runtime_error("File not opened for writing");
        }

        int32_t frame_number = clusters.frame_number();
        if (fwrite(&frame_number, sizeof(frame_number), 1, m_fp.get()) != 1) {
            throw std::runtime_error(LOCATION + "Could not write frame number");
        }

        uint32_t n_clusters = clusters.size();
        if (fwrite(&n_clusters, sizeof(n_clusters), 1, m_fp.get()) != 1) {
            throw std::runtime_error(LOCATION +
                                     "Could not write number of clusters");
        }

        if (fwrite(clusters.data(), clusters.item_size(), clusters.size(),
                   m_fp.get()) != clusters.size()) {
            throw std::runtime_error(LOCATION + "Could not write clusters");
        }
    }

    /**
     * @brief Return the number of clusters requested by each iterator step.
     */
    size_t chunk_size() const { return m_chunk_size; }

    /**
     * @brief Estimate the number of clusters in the file from its size.
     *
     * Frame-header bytes are included in the estimate, so it may exceed the
     * actual number of clusters. The file position is not changed.
     */
    size_t estimate_n_clusters() const {
        return std::filesystem::file_size(m_filename) / sizeof(ClusterType);
    }

    /**
     * @brief Select clusters by their center coordinate when reading.
     * @param roi Half-open region of interest: [xmin, xmax) x [ymin, ymax).
     */
    void set_roi(ROI roi) { m_roi = roi; }

    /**
     * @brief Discard clusters that do not pass the noise thresholds.
     * @param noise_map Per-pixel noise indexed as [y, x]. The map is copied.
     * A cluster is retained only when its central pixel exceeds the local
     * noise, its highest 2x2 sum exceeds twice the noise, and its total sum
     * exceeds three times the noise.
     * @warning The map must cover every cluster center coordinate in the file.
     */
    void set_noise_map(const NDView<int32_t, 2> noise_map) {
        m_noise_map = NDArray<int32_t, 2>(noise_map);
    }

    /**
     * @brief Apply a gain map to clusters selected while reading.
     * @param gain_map Per-pixel gain in ADU/energy, indexed as [y, x]. The map
     * is copied and inverted internally.
     * @note Clusters whose complete footprint extends beyond the gain map are
     * retained with all cluster data values set to zero.
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
     * @brief Close the file.
     *
     * Calling close more than once is safe. The destructor closes an open file
     * automatically.
     */
    void close() {
        m_fp = FilePtr{};
        m_mode = "";
    }

    /**
     * @brief Return the current byte position in the file.
     * @throws std::runtime_error If the file is closed or its position cannot
     * be determined.
     */
    int64_t tell() {
        if (!m_fp.get()) {
            throw std::runtime_error(LOCATION + "File not opened");
        }
        return m_fp.tell();
    }

  private:
    /** @brief Open the file in specific mode
     *
     */
    void open(const std::string &mode) {
        close();

        if (mode == "r") {
            m_fp = FilePtr(m_filename, "rb");
            m_mode = "r";
        } else if (mode == "w") {
            m_fp = FilePtr(m_filename, "wb");
            m_mode = "w";
        } else if (mode == "a") {
            m_fp = FilePtr(m_filename, "ab");
            m_mode = "a";
        } else {
            throw std::runtime_error("Unsupported mode: " + mode);
        }
    }
    ClusterVector<ClusterType> read_clusters_with_cut(size_t n_clusters);
    ClusterVector<ClusterType> read_clusters_without_cut(size_t n_clusters);
    bool read_frame_header(int32_t &frame_number, uint32_t &n_clusters);
    bool read_frame_with_cut(ClusterVector<ClusterType> &clusters);
    bool read_frame_without_cut(ClusterVector<ClusterType> &clusters);
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
        nph_read +=
            fread((buf + nph_read), clusters.item_size(), nn, m_fp.get());
        m_num_left = nph - nn; // write back the number of photons left
    }

    if (nph_read < n_clusters) {
        // keep on reading frames and photons until reaching n_clusters
        while (fread(&iframe, sizeof(iframe), 1, m_fp.get())) {
            clusters.set_frame_number(iframe);
            // read number of clusters in frame
            if (fread(&nph, sizeof(nph), 1, m_fp.get())) {
                if (nph > (n_clusters - nph_read))
                    nn = n_clusters - nph_read;
                else
                    nn = nph;

                nph_read += fread((buf + nph_read), clusters.item_size(), nn,
                                  m_fp.get());
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
        while (fread(&frame_number, sizeof(frame_number), 1, m_fp.get())) {
            if (fread(&m_num_left, sizeof(m_num_left), 1, m_fp.get())) {
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
    auto rc = fread(&c, sizeof(c), 1, m_fp.get());
    if (rc != 1) {
        throw std::runtime_error(LOCATION + "Could not read cluster");
    }
    --m_num_left;
    return c;
}

template <typename ClusterType, typename Enable>
bool ClusterFile<ClusterType, Enable>::read_frame_header(int32_t &frame_number,
                                                         uint32_t &n_clusters) {
    const auto frame_number_bytes =
        fread(&frame_number, 1, sizeof(frame_number), m_fp.get());
    if (frame_number_bytes == 0 && feof(m_fp.get()) && !ferror(m_fp.get())) {
        return false;
    }
    if (frame_number_bytes != sizeof(frame_number)) {
        if (ferror(m_fp.get())) {
            throw std::runtime_error(LOCATION + "Error reading from file");
        }
        throw std::runtime_error(LOCATION + "Incomplete frame number");
    }

    const auto cluster_count_bytes =
        fread(&n_clusters, 1, sizeof(n_clusters), m_fp.get());
    if (cluster_count_bytes != sizeof(n_clusters)) {
        if (ferror(m_fp.get())) {
            throw std::runtime_error(LOCATION + "Error reading from file");
        }
        throw std::runtime_error(LOCATION + "Incomplete number of clusters");
    }
    return true;
}

template <typename ClusterType, typename Enable>
bool ClusterFile<ClusterType, Enable>::read_frame_without_cut(
    ClusterVector<ClusterType> &clusters) {
    int32_t frame_number;
    uint32_t n_clusters;
    if (!read_frame_header(frame_number, n_clusters)) {
        return false;
    }

    LOG(logDEBUG1) << "Reading " << n_clusters << " clusters from frame "
                   << frame_number;

    clusters.set_frame_number(frame_number);
    clusters.resize(n_clusters);

    LOG(logDEBUG1) << "clusters.item_size(): " << clusters.item_size();

    if (fread(clusters.data(), clusters.item_size(), n_clusters, m_fp.get()) !=
        static_cast<size_t>(n_clusters)) {
        throw std::runtime_error(LOCATION + "Could not read clusters");
    }

    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);
    return true;
}

template <typename ClusterType, typename Enable>
bool ClusterFile<ClusterType, Enable>::read_frame_with_cut(
    ClusterVector<ClusterType> &clusters) {
    int32_t frame_number;
    uint32_t n_clusters;
    if (!read_frame_header(frame_number, n_clusters)) {
        return false;
    }

    m_num_left = n_clusters;
    clusters.resize(0);
    clusters.reserve(n_clusters);
    clusters.set_frame_number(frame_number);
    while (m_num_left) {
        ClusterType c = read_one_cluster();
        if (is_selected(c)) {
            clusters.push_back(c);
        }
    }
    if (m_gain_map)
        m_gain_map->apply_gain_map(clusters);
    return true;
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
        auto sum_2x2 = cl.max_sum_2x2().sum; // highest sum of 2x2 subclusters
        auto total_sum = cl.sum();           // sum of all pixels

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
