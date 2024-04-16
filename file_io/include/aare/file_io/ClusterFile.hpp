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
 *  int32_t data[9];
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
    ClusterFileConfig(int32_t frame_number_, int32_t n_clusters_)
        : frame_number(frame_number_), n_clusters(n_clusters_) {}
    ClusterFileConfig() : frame_number(0), n_clusters(0) {}
    bool operator==(const ClusterFileConfig &other) const {
        return frame_number == other.frame_number && n_clusters == other.n_clusters;
    }
    bool operator!=(const ClusterFileConfig &other) const { return !(*this == other); }
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) + " n_clusters: " + std::to_string(n_clusters) + "\n";
    }
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
};

} // namespace aare