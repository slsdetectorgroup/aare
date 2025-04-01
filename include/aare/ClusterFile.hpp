#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fstream>
#include <optional>

namespace aare {


//TODO! Legacy enums, migrate to enum class
typedef enum {
    cBottomLeft = 0,
    cBottomRight = 1,
    cTopLeft = 2,
    cTopRight = 3
} corner;

typedef enum {
    pBottomLeft = 0,
    pBottom = 1,
    pBottomRight = 2,
    pLeft = 3,
    pCenter = 4,
    pRight = 5,
    pTopLeft = 6,
    pTop = 7,
    pTopRight = 8
} pixel;

struct Eta2 {
    double x;
    double y;
    corner c;
    int32_t sum;
};

struct ClusterAnalysis {
    uint32_t c;
    int32_t tot;
    double etax;
    double etay;
};



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
class ClusterFile {
    FILE *fp{};
    uint32_t m_num_left{};            /*Number of photons left in frame*/
    size_t m_chunk_size{};            /*Number of clusters to read at a time*/
    const std::string m_mode;         /*Mode to open the file in*/
    std::optional<ROI> m_roi;         /*Region of interest, will be applied if set*/
    std::optional<NDArray<int32_t, 2>> m_noise_map; /*Noise map to cut photons, will be applied if set*/
    std::optional<NDArray<double, 2>> m_gain_map; /*Gain map to apply to the clusters, will be applied if set*/

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
    ClusterVector<int32_t> read_clusters(size_t n_clusters);

    ClusterVector<int32_t> read_clusters(size_t n_clusters, ROI roi);

    /**
     * @brief Read a single frame from the file and return the clusters. The
     * cluster vector will have the frame number set.
     * @throws std::runtime_error if the file is not opened for reading or the file pointer not
     * at the beginning of a frame
     */
    ClusterVector<int32_t> read_frame();


    void write_frame(const ClusterVector<int32_t> &clusters);
    
    /**
     * @brief Return the chunk size
     */
    size_t chunk_size() const { return m_chunk_size; }

    /**
     * @brief Set the region of interest to use when reading clusters. If set only clusters within
     * the ROI will be read.
     */
    void set_roi(ROI roi);

    /**
     * @brief Set the noise map to use when reading clusters. If set clusters below the noise
     * level will be discarded. Selection criteria one of: Central pixel above noise, highest
     * 2x2 sum above 2 * noise, total sum above 3 * noise.
     */
    void set_noise_map(const NDView<int32_t, 2> noise_map);

    /**
     * @brief Set the gain map to use when reading clusters. If set the gain map will be applied
     * to the clusters that pass ROI and noise_map selection.
     */
    void set_gain_map(const NDView<double, 2> gain_map);
    
    
    /**
     * @brief Close the file. If not closed the file will be closed in the destructor
     */
    void close();

    private:
        ClusterVector<int32_t> read_clusters_with_cut(size_t n_clusters);
        ClusterVector<int32_t> read_clusters_without_cut(size_t n_clusters);
        ClusterVector<int32_t> read_frame_with_cut();
        ClusterVector<int32_t> read_frame_without_cut();
        bool is_selected(Cluster3x3 &cl);
        Cluster3x3 read_one_cluster();
};

//TODO! helper functions that doesn't really belong here
NDArray<double, 2> calculate_eta2(ClusterVector<int> &clusters);
Eta2 calculate_eta2(Cluster3x3 &cl);
Eta2 calculate_eta2(Cluster2x2 &cl);


} // namespace aare
