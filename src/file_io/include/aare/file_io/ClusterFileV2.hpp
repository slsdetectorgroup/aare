#pragma once
#include "aare/core/defs.hpp"
#include <filesystem>
#include <string>

namespace aare {
struct ClusterHeader {
    int16_t frame_number;
    int16_t n_clusters;
};

struct ClusterV2_ {
    int16_t x;
    int16_t y;
    std::array<int32_t, 9> data;
};

struct ClusterV2 {
    ClusterV2_ cluster;
    int16_t frame_number;
};

class ClusterFileV2 {
  private:
    bool m_closed = true;
    std::filesystem::path m_fpath;
    std::string m_mode;
    FILE *fp;

  public:
    ClusterFileV2(std::filesystem::path const &fpath, std::string const &mode) {
        m_fpath = fpath;
        m_mode = mode;
        fp = fopen(fpath.c_str(), "rb");
        m_closed = false;
    }
    ~ClusterFileV2() { close(); }
    std::vector<ClusterV2> read() {
        ClusterHeader header;
        fread(&header, sizeof(ClusterHeader), 1, fp);
        std::vector<ClusterV2_> clusters_(header.n_clusters);
        fread(clusters_.data(), sizeof(ClusterV2_), header.n_clusters, fp);
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
    
    void close() {
        if (!m_closed) {
            fclose(fp);
            m_closed = true;
        }
    }
};
} // namespace aare