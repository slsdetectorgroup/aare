#pragma once
#include "aare/FileFactory.hpp"
#include "aare/NumpyFile.hpp"
#include "aare/defs.hpp"
#include <fstream>



template <DetectorType detector, typename DataType> class NumpyFileFactory : public FileFactory<detector, DataType> {
  private:
    std::ifstream f;
    void read_data(File<detector, DataType> *_file);

  public:
    NumpyFileFactory(std::filesystem::path fpath);
    void parse_metadata(File<detector, DataType> *_file) override;
    File<detector, DataType> *load_file() override;
    void parse_fname(File<detector, DataType> *){};

};