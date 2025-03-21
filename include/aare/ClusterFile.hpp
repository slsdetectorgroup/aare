#pragma once

#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <filesystem>
#include <fstream>

namespace aare {

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
struct Cluster {
    CoordType x;
    CoordType y;
    T data[ClusterSizeX * ClusterSizeY];
};

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

/*
Binary cluster file. Expects data to be layed out as:
int32_t frame_number
uint32_t number_of_clusters
int16_t x, int16_t y, int32_t data[9] x number_of_clusters
int32_t frame_number
uint32_t number_of_clusters
....
*/

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
    ClusterVector<int32_t> read_clusters(size_t n_clusters);

    ClusterVector<int32_t> read_clusters(size_t n_clusters, ROI roi);

    /**
     * @brief Read a single frame from the file and return the clusters. The
     * cluster vector will have the frame number set.
     * @throws std::runtime_error if the file is not opened for reading or the
     * file pointer not at the beginning of a frame
     */
    ClusterVector<int32_t> read_frame();

    void write_frame(const ClusterVector<int32_t> &clusters);

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

int analyze_data(int32_t *data, int32_t *t2, int32_t *t3, char *quad,
                 double *eta2x, double *eta2y, double *eta3x, double *eta3y);
int analyze_cluster(Cluster3x3 &cl, int32_t *t2, int32_t *t3, char *quad,
                    double *eta2x, double *eta2y, double *eta3x, double *eta3y);

template <typename ClusterType>
NDArray<double, 2> calculate_eta2(ClusterVector<ClusterType> &clusters);

template <typename T> Eta2 calculate_eta2(Cluster<T, 3, 3> &cl);

Eta2 calculate_eta2(Cluster2x2 &cl);

template <typename ClusterType> Eta2 calculate_eta2(ClusterType &cl);

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
Eta2 calculate_eta2(Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl);

} // namespace aare
