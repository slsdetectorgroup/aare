#pragma once
#include "aare/core/defs.hpp"
#include "aare/file_io/FileFactory.hpp"
#include "aare/file_io/NumpyFile.hpp"
#include <fstream>

namespace aare {

class NumpyFileFactory : public FileFactory {
  private:
    std::ifstream f;
    void read_data(FileInterface *_file);

  public:
    NumpyFileFactory(std::filesystem::path fpath);
    void parse_metadata(FileInterface *_file) override{/*TODO! remove after refactor*/};
    NumpyFile *load_file_read() override;
    NumpyFile *load_file_write(FileConfig) override;
    void parse_fname(FileInterface *) override{};
};

} // namespace aare