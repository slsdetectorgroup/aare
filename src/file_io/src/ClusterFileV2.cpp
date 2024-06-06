#include "aare/file_io/ClusterFileV2.hpp"

namespace aare {

ClusterFileV2::ClusterFileV2(std::filesystem::path const &fpath, std::string const &mode,
                             ClusterFileConfig const &config)
    : m_fpath(fpath), m_mode(mode) {
    if (mode == "w") {
        fp = fopen(fpath.c_str(), "wb");
        write_header(config);
    } else if (mode == "r") {
        fp = fopen(fpath.c_str(), "rb");
        read_header();
    } else {
        throw std::runtime_error("invalid mode");
    }
}
} // namespace aare