#pragma once
#include "aare/core/defs.hpp"

#include <filesystem>
#include <string>

/**
 * cluster file format:
 * header: [int32 frame_number][int32 n_clusters]
 * data: [clusters....]
 * where each cluster is of the form:
 * typedef struct {
 *  int16_t x;
 *  int16_t y;
 *  int32_t data[n];
 *} Cluster ;
 *
 */

namespace aare {

/**
 * @brief Configuration of the ClusterFile
 * can be use as the header of the cluster file
 */
struct ClusterFileConfig {
    int32_t frame_number;
    int32_t n_clusters;
    int cluster_sizeX;
    int cluster_sizeY;
    DType dt;
    ClusterFileConfig(int32_t frame_number_, int32_t n_clusters_, int cluster_sizeX_ = 3, int cluster_sizeY_ = 3,
                      DType dt_ = DType::INT32)
        : frame_number{frame_number_}, n_clusters{n_clusters_}, cluster_sizeX{cluster_sizeX_},
          cluster_sizeY{cluster_sizeY_}, dt{dt_} {}
    ClusterFileConfig() : ClusterFileConfig(0, 0) {}
    bool operator==(const ClusterFileConfig &other) const {
        return frame_number == other.frame_number && n_clusters == other.n_clusters && dt == other.dt &&
               cluster_sizeX == other.cluster_sizeX && cluster_sizeY == other.cluster_sizeY;
    }
    bool operator!=(const ClusterFileConfig &other) const { return !(*this == other); }
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) + ", n_clusters: " + std::to_string(n_clusters) +
               ", dt: " + dt.to_string() + "\n cluster_sizeX: " + std::to_string(cluster_sizeX) +
               ", cluster_sizeY: " + std::to_string(cluster_sizeY);
    }
};

struct ClusterFileHeader {
    int32_t frame_number;
    int32_t n_clusters;
};

/**
 * @brief Class to read and write clusters to a file
 */
class ClusterFile {

  public:
    ClusterFile(const std::filesystem::path &fname, const std::string &mode, ClusterFileConfig config = {});
    void write(std::vector<Cluster> &clusters);
    void write(Cluster &cluster);
    Cluster read();
    Cluster iread(size_t cluster_number);
    std::vector<Cluster> read(size_t n_clusters);
    void seek(size_t cluster_number);
    size_t tell() const;
    size_t count() noexcept;
    int32_t frame() const;
    void update_header() /* throws */;
    ~ClusterFile() noexcept;

  private:
    FILE *fp = nullptr;
    // size_t current_cluster{};
    std::filesystem::path fname{};
    std::string mode{};
    int32_t frame_number{};
    int32_t n_clusters{};
    static const int HEADER_BYTES = 8;
    DType dt;
    size_t m_cluster_size{};
};

} // namespace aare