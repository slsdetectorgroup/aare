#pragma once
#include <string>
#include <filesystem>
#include "aare/core/defs.hpp"

namespace aare{
    /*
     * ClusterV2 file format:
     * 1. Header:
     *     - magic number: 4 bytes
     *    - version: 2 bytes
     * 
    */

    class ClusterFileV2 {
    public:
        ClusterFileV2(std::filesystem::path const &fpath, std::string const &mode, ClusterFileConfig const &config);
        ~ClusterFileV2();
        void write(std::vector<Cluster> const &clusters);
        std::vector<Cluster> read();
        void close();
    private:
        void write_header();
        void read_header();
        std::filesystem::path m_fpath;
        std::string m_mode;
        FILE *fp;

    };
}