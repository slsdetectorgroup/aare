#pragma once
#include "aare/core/defs.hpp"
#include <filesystem>
#include <string>

namespace aare {
namespace deprecated {

struct ClusterHeader {
    int32_t frame_number;
    int32_t n_clusters;
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) +
               ", n_clusters: " + std::to_string(n_clusters);
    }
};

struct Cluster_ {
    int16_t x;
    int16_t y;
    std::array<int32_t, 9> data;
    std::string to_string(bool detailed = false) const {
        if (detailed) {
            std::string data_str = "[";
            for (auto &d : data) {
                data_str += std::to_string(d) + ", ";
            }
            data_str += "]";
            return "x: " + std::to_string(x) + ", y: " + std::to_string(y) + ", data: " + data_str;
        }
        return "x: " + std::to_string(x) + ", y: " + std::to_string(y);
    }
};

struct ClusterV2 {
    Cluster_ cluster;
    int32_t frame_number;
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) + ", " + cluster.to_string();
    }
};

/**
 * @brief
 * important not: fp always points to the clutsers header and does not point to individual clusters
 *
 */
class ClusterFile {
  private:
    bool m_closed = true;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *fp;

  public:
    ClusterFile(std::filesystem::path const &fpath, std::string const &mode) {
        if (mode != "r" && mode != "w")
            throw std::invalid_argument("mode must be 'r' or 'w'");
        if (mode == "r" && !std::filesystem::exists(fpath))
            throw std::invalid_argument("File does not exist");
        m_fpath = fpath;
        m_mode = mode;
        if (mode == "r") {
            fp = fopen(fpath.string().c_str(), "rb");
        } else if (mode == "w") {
            if (std::filesystem::exists(fpath)) {
                fp = fopen(fpath.string().c_str(), "r+b");
            } else {
                fp = fopen(fpath.string().c_str(), "wb");
            }
        }
        if (fp == nullptr) {
            throw std::runtime_error("Failed to open file");
        }
        m_closed = false;
    }
    ~ClusterFile() { close(); }
    std::vector<ClusterV2> read() {
        ClusterHeader header;
        fread(&header, sizeof(ClusterHeader), 1, fp);
        std::vector<Cluster_> clusters_(header.n_clusters);
        fread(clusters_.data(), sizeof(Cluster_), header.n_clusters, fp);
        std::vector<ClusterV2> clusters;
        for (auto &c : clusters_) {
            ClusterV2 cluster;
            cluster.cluster = std::move(c);
            cluster.frame_number = header.frame_number;
            clusters.push_back(cluster);
        }

        return clusters;
    }
    std::vector<std::vector<ClusterV2>> read(int n_frames) {
        std::vector<std::vector<ClusterV2>> clusters;
        for (int i = 0; i < n_frames; i++) {
            clusters.push_back(read());
        }
        return clusters;
    }

    size_t write(std::vector<ClusterV2> const &clusters) {
        if (m_mode != "w") {
            throw std::runtime_error("File not opened in write mode");
        }
        if (clusters.empty()) {
            return 0;
        }
        ClusterHeader header;
        header.frame_number = clusters[0].frame_number;
        header.n_clusters = clusters.size();
        fwrite(&header, sizeof(ClusterHeader), 1, fp);
        for (auto &c : clusters) {
            fwrite(&c.cluster, sizeof(Cluster_), 1, fp);
        }
        return clusters.size();
    }

    size_t write(std::vector<std::vector<ClusterV2>> const &clusters) {
        if (m_mode != "w") {
            throw std::runtime_error("File not opened in write mode");
        }
        size_t n_clusters = 0;
        for (auto &c : clusters) {
            n_clusters += write(c);
        }
        return n_clusters;
    }

    int seek_to_begin() { return fseek(fp, 0, SEEK_SET); }
    int seek_to_end() { return fseek(fp, 0, SEEK_END); }

    int32_t frame_number() {
        auto pos = ftell(fp);
        ClusterHeader header;
        fread(&header, sizeof(ClusterHeader), 1, fp);
        fseek(fp, pos, SEEK_SET);
        return header.frame_number;
    }

    void close() {
        if (!m_closed) {
            fclose(fp);
            m_closed = true;
        }
    }
};
} // namespace deprecated
} // namespace aare