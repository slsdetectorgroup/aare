#pragma once
#include "aare/FileFactory.hpp"
#include "aare/NumpyFile.hpp"
#include "aare/defs.hpp"
#include <fstream>



class NumpyFileFactory : public FileFactory {
  private:
    std::ifstream f;
    void read_data(File *_file);

  public:
    NumpyFileFactory(std::filesystem::path fpath);
    void parse_metadata(File *_file) override;
    NumpyFile* load_file() override;
    void parse_fname(File*){};

};