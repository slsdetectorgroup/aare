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
    : fname{fname_}, mode{mode_}, frame_number{config.frame_number}, n_clusters{config.n_clusters}, dt{config.dt} {

    // check if the file has the .clust extension
    if (fname.extension() != ".clust") {
        aare::logger::warn("file", fname, "does not have .clust extension");
    }

    if (mode == "r") {
        // check if the file exists and is a regular file
        if (not std::filesystem::exists(fname)) {
            throw std::invalid_argument(fmt::format("file {} does not exist", fname.c_str()));
        }
        if (not std::filesystem::is_regular_file(fname)) {
            throw std::invalid_argument(fmt::format("file {} is not a regular file", fname.c_str()));
        }
        if (dt == DType(DType::TypeIndex::ERROR)) {
            throw std::invalid_argument("data type not set in ClusterFileConfig");
        }
        // open file
        fp = fopen(fname.c_str(), "rb");
        if (fp == nullptr) {
            throw std::runtime_error(fmt::format("could not open file {}", fname.c_str()));
        }
        // read header
        ClusterFileHeader header;
        const size_t rc = fread(&header, sizeof(header), 1, fp);
        if (rc != 1) {
            throw std::runtime_error(fmt::format("could not read header from file {}", fname.c_str()));
        }
        frame_number = header.frame_number;
        n_clusters = header.n_clusters;
    } else if (mode == "w") {
        // open file
        fp = fopen(fname.c_str(), "wb");
        if (fp == nullptr) {
            throw std::runtime_error(fmt::format("could not open file {}", fname.c_str()));
        }

        // write header
        ClusterFileHeader header{config.frame_number, config.n_clusters};
        if (fwrite(&header, sizeof(header), 1, fp) != 1) {
            throw std::runtime_error(fmt::format("could not write header to file {}", fname.c_str()));
        }
    } else {
        throw std::invalid_argument("mode must be 'r' or 'w'");
    }

    m_cluster_size = 2 * sizeof(int16_t) + config.cluster_sizeX * config.cluster_sizeY * dt.bytes();
}
/**
 * @brief Writes a vector of clusters to the file.
 *
 * @param clusters The vector of clusters to write to the file.
 */
void ClusterFile::write(std::vector<Cluster> &clusters) {
    if (clusters.size() == 0) {
        return;
    }
    assert(clusters[0].dt == dt && "cluster data type mismatch");

    // prepare buffer to write to file
    auto bytes_per_pixel = clusters[0].dt.bytes();
    auto buffer = new std::byte[m_cluster_size * clusters.size()];
    for (size_t i = 0; i < clusters.size(); i++) {
        auto &cluster = clusters[i];
        memcpy(buffer + i * m_cluster_size, &cluster.x, sizeof(int16_t));
        memcpy(buffer + i * m_cluster_size + sizeof(int16_t), &cluster.y, sizeof(int16_t));
        memcpy(buffer + i * m_cluster_size + 2 * sizeof(int16_t), cluster.data(), cluster.size());
    }
    fwrite(buffer, m_cluster_size * clusters.size(), 1, fp);
    delete[] buffer;
}
/**
 * @brief Writes a single cluster to the file.
 *
 * @param cluster The cluster to write to the file.
 */
void ClusterFile::write(Cluster &cluster) {
    // prepare buffer to write to file
    auto buffer = new std::byte[m_cluster_size];
    memcpy(buffer, &cluster.x, sizeof(int16_t));
    memcpy(buffer + sizeof(int16_t), &cluster.y, sizeof(int16_t));
    memcpy(buffer + 2 * sizeof(int16_t), cluster.data(), cluster.size());
    fwrite(buffer, m_cluster_size, 1, fp);
    delete[] buffer;
}
/**
 * @brief Reads a single cluster from the file.
 *
 * @return The cluster read from the file.
 */
Cluster ClusterFile::read() {
    if (tell() >= count()) {
        throw std::runtime_error("cluster number out of range");
    }

    Cluster cluster(3, 3, DType::INT32);
    auto tmp = new std::byte[cluster.size() + 2 * sizeof(int16_t)];
    fread(tmp, cluster.size() + 2 * sizeof(int16_t), 1, fp);
    memcpy(&cluster.x, tmp, sizeof(int16_t));
    memcpy(&cluster.y, tmp + sizeof(int16_t), sizeof(int16_t));
    memcpy(cluster.data(), tmp + 2 * sizeof(int16_t), cluster.size());
    delete[] tmp;
    return cluster;
}

/**
 * @brief Reads a specific cluster from the file.
 *
 * The file pointer is moved to the specific cluster
 *
 * @param cluster_number The number of the cluster to read from the file.
 * @return The cluster read from the file.
 */
Cluster ClusterFile::iread(size_t cluster_number) {
    if (cluster_number >= count()) {
        throw std::runtime_error("cluster number out of range");
    }

    auto old_pos = ftell(fp);
    this->seek(cluster_number);
    Cluster cluster = read();
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
    if (n_clusters_ + tell() > count()) {
        throw std::runtime_error("cluster number out of range");
    }
    std::vector<Cluster> clusters(n_clusters_, Cluster(3, 3, DType::INT32));

    // TODO: read all clusters at once if possible
    for (size_t i = 0; i < n_clusters_; i++) {
        clusters[i] = read();
    }
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
    if (cluster_number > count()) {
        throw std::runtime_error("cluster number out of range");
    }
    const auto offset = static_cast<int64_t>(sizeof(ClusterFileHeader) + cluster_number * m_cluster_size);
    fseek(fp, offset, SEEK_SET);
}

/**
 * @brief Gets the current position of the file pointer in terms of clusters.
 *
 * The position is calculated as the number of clusters from the beginning of the file to the current position of
 * the file pointer.
 *
 * @return The current position of the file pointer in terms of clusters.
 */
size_t ClusterFile::tell() const { return (ftell(fp) - sizeof(ClusterFileHeader)) / m_cluster_size; }

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
    const size_t n_clusters_ = (ftell(fp) - sizeof(ClusterFileHeader)) / m_cluster_size;
    // restore the file position
    fseek(fp, old_pos, SEEK_SET);
    return n_clusters_;
}

int32_t ClusterFile::frame() const { return frame_number; }
void ClusterFile::update_header() {
    if (mode == "r") {
        throw std::runtime_error("update header is not implemented for read mode");
    }
    // update the header with the correct number of clusters
    aare::logger::debug("updating header with correct number of clusters", count());
    auto tmp_n_clusters = count();
    fseek(fp, 0, SEEK_SET);
    ClusterFileHeader header{frame_number, static_cast<int32_t>(tmp_n_clusters)};
    if (fwrite(&header, sizeof(ClusterFileHeader), 1, fp) != 1) {
        throw std::runtime_error("could not write header to file");
    }
    if (fflush(fp) != 0) {
        throw std::runtime_error("could not flush file");
    }
}
ClusterFile::~ClusterFile() noexcept {
    if (mode == "w") {
        try {
            update_header();
        } catch (std::runtime_error &e) {
            aare::logger::error("error updating header", e.what());
        }
    }

    if (fp != nullptr) {
        fclose(fp);
    }
}

} // namespace aare