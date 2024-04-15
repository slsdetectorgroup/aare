#include "aare/file_io/ClusterFile.hpp"
#include "aare/core/defs.hpp"
#include "aare/utils/logger.hpp"
#include "fmt/core.h"
namespace aare {

/**
 * @brief Constructor for the ClusterFile class.
 *
 * Opens the file with the given mode ('r' for read, 'w' for write).
 * Throws an exception if the mode is not 'r' or 'w', or if the file cannot be opened.
 *
 * @param fname_ The name of the file to open.
 * @param mode_ The mode to open the file in.
 * @param config Configuration for the file header.
 */
ClusterFile::ClusterFile(const std::filesystem::path &fname_, const std::string &mode_, ClusterFileConfig config)
    : fname{fname_}, mode{mode_} {

    // check if the file exists and is a regular file
    if (not std::filesystem::exists(fname)) {
        throw std::invalid_argument(fmt::format("file {} does not exist", fname.c_str()));
    }
    if (not std::filesystem::is_regular_file(fname)) {
        throw std::invalid_argument(fmt::format("file {} is not a regular file", fname.c_str()));
    }
    // check if the file size is a multiple of the cluster size
    if ((std::filesystem::file_size(fname) - HEADER_BYTES) % sizeof(Cluster) != 0) {
        aare::logger::warn("file", fname, "size is not a multiple of cluster size");
    }
    // check if the file has the .clust extension
    if (fname.extension() != ".clust") {
        aare::logger::warn("file", fname, "does not have .clust extension");
    }

    if (mode == "r") {
        if (config != ClusterFileConfig()) {
            aare::logger::warn("ignored ClusterFileConfig for read mode");
        }
        // open file
        fp = fopen(fname.c_str(), "rb");
        if (fp == nullptr) {
            throw std::runtime_error(fmt::format("could not open file {}", fname.c_str()));
        }
        // read header
        const size_t rc = fread(&config, sizeof(config), 1, fp);
        if (rc != 1) {
            throw std::runtime_error(fmt::format("could not read header from file {}", fname.c_str()));
        }
        n_clusters = config.n_clusters;
        frame_number = config.frame_number;

    } else if (mode == "w") {
        // open file
        fp = fopen(fname.c_str(), "wb");
        if (fp == nullptr) {
            throw std::runtime_error(fmt::format("could not open file {}", fname.c_str()));
        }

        // write header
        if (fwrite(&config, sizeof(config), 1, fp) != 1) {
            throw std::runtime_error(fmt::format("could not write header to file {}", fname.c_str()));
        }
    } else {
        throw std::invalid_argument("mode must be 'r' or 'w'");
    }
}
/**
 * @brief Writes a vector of clusters to the file.
 *
 * Each cluster is written as a binary block of size sizeof(Cluster).
 *
 * @param clusters The vector of clusters to write to the file.
 */
void ClusterFile::write(std::vector<Cluster> &clusters) {
    fwrite(clusters.data(), sizeof(Cluster), clusters.size(), fp);
}
/**
 * @brief Writes a single cluster to the file.
 *
 * The cluster is written as a binary block of size sizeof(Cluster).
 *
 * @param cluster The cluster to write to the file.
 */
void ClusterFile::write(Cluster &cluster) { fwrite(&cluster, sizeof(Cluster), 1, fp); }
/**
 * @brief Reads a single cluster from the file.
 *
 * The cluster is read as a binary block of size sizeof(Cluster).
 *
 * @return The cluster read from the file.
 */
Cluster ClusterFile::read() {

    Cluster cluster{};
    fread(&cluster, sizeof(Cluster), 1, fp);
    return cluster;
}
void verify_range(size_t cluster_number, size_t n_clusters_) {
    if (cluster_number > n_clusters_) {
        throw std::invalid_argument(fmt::format(
            "cluster_number {} is greater than the number of clusters in the file {}", cluster_number, n_clusters_));
    }
}

/**
 * @brief Reads a specific cluster from the file.
 *
 * The file pointer is moved to the specific cluster, and the cluster is read as a binary block of size sizeof(Cluster).
 *
 * @param cluster_number The number of the cluster to read from the file.
 * @return The cluster read from the file.
 */
Cluster ClusterFile::iread(size_t cluster_number) {
    verify_range(n_clusters, count());

    auto old_pos = ftell(fp);
    this->seek(cluster_number);
    Cluster cluster{};
    fread(&cluster, sizeof(Cluster), 1, fp);
    fseek(fp, old_pos, SEEK_SET); // restore the file position
    return cluster;
}

/**
 * @brief Reads a specific number of clusters from the file.
 *
 * Each cluster is read as a binary block of size sizeof(Cluster).
 *
 * @param n_clusters The number of clusters to read from the file.
 * @return A vector of clusters read from the file.
 */
std::vector<Cluster> ClusterFile::read(size_t n_clusters_) {
    verify_range(n_clusters_ - 1, count());
    std::vector<Cluster> clusters(n_clusters_);
    fread(clusters.data(), sizeof(Cluster), n_clusters, fp);
    return clusters;
}

/**
 * @brief Moves the file pointer to a specific cluster.
 *
 * The file pointer is moved to the start of the specific cluster, based on the size of a cluster.
 *
 * @param cluster_number The number of the cluster to move the file pointer to.
 */
void ClusterFile::seek(size_t cluster_number) {
    verify_range(n_clusters, count());

    const auto offset = static_cast<int64_t>(sizeof(ClusterFileConfig) + cluster_number * sizeof(Cluster));

    fseek(fp, offset, SEEK_SET);
}

/**
 * @brief Gets the current position of the file pointer in terms of clusters.
 *
 * The position is calculated as the number of clusters from the beginning of the file to the current position of the
 * file pointer.
 *
 * @return The current position of the file pointer in terms of clusters.
 */
size_t ClusterFile::tell() const { return ftell(fp) / sizeof(Cluster); }

/**
 * @brief Counts the number of clusters in the file.
 *
 * The count is calculated as the size of the file divided by the size of a cluster.
 *
 * @return The number of clusters in the file.
 */
size_t ClusterFile::count() noexcept {
    if (mode == "r") {
        return n_clusters;
    }
    // save the current position
    auto old_pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    const size_t n_clusters_ = ftell(fp) / sizeof(Cluster);
    // restore the file position
    fseek(fp, old_pos, SEEK_SET);
    return n_clusters_;
}

int32_t ClusterFile::frame() const { return frame_number; }

ClusterFile::~ClusterFile() noexcept {
    if (mode == "w") {
        // update the header with the correct number of clusters
        aare::logger::info("updating header with correct number of clusters", count());
        auto tmp_n_clusters = count();
        fseek(fp, 0, SEEK_SET);
        ClusterFileConfig config(frame_number, static_cast<int32_t>(tmp_n_clusters));
        if (fwrite(&config, sizeof(config), 1, fp) != 1) {
            aare::logger::warn("could not write header to file");
        }
    }

    if (fp != nullptr) {
        fclose(fp);
    }
}

} // namespace aare