#include "aare/NumpyFileFactory.hpp"
#include "aare/NumpyHelpers.hpp"

namespace aare {

NumpyFileFactory::NumpyFileFactory(std::filesystem::path fpath) { this->m_fpath = fpath; }

NumpyFile *NumpyFileFactory::load_file_read() {
    NumpyFile *file = new NumpyFile(this->m_fpath);
    return file;
};

NumpyFile *NumpyFileFactory::load_file_write(FileConfig config) {
    NumpyFile *file = new NumpyFile(config, {config.dtype, false, {config.rows, config.cols}});

    return file;
};

} // namespace aare