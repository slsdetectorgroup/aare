#pragma once
#include "aare/core/DType.hpp"
#include "aare/file_io/FileInterface.hpp"
#include "aare/utils/logger.hpp"
#include <filesystem>

namespace aare {

class FileFactory {
    // Class that will be used to create FileInterface objects
    // follows the factory pattern
  protected:
    std::filesystem::path m_fpath;

  public:
    static FileFactory *get_factory(std::filesystem::path);
    // virtual int deleteFile() = 0;
    static FileInterface *load_file(std::filesystem::path p, std::string mode, FileConfig cfg = {}) {
        if ((mode == "r" or mode == "a") and not std::filesystem::exists(p)) {
            throw std::runtime_error(LOCATION + "File does not exist");
        }

        auto factory = get_factory(p);

        FileInterface *tmp = nullptr;
        if (mode == "r") {
            tmp = factory->load_file_read();
        } else if (mode == "w") {
            tmp = factory->load_file_write(cfg);
        }
        delete factory;
        return tmp;
    };
    virtual FileInterface *load_file_read() = 0; // TODO: add option to load all file to memory or keep it on disk
    virtual FileInterface *load_file_write(FileConfig) = 0;
    virtual void parse_metadata(FileInterface *) = 0;
    virtual void parse_fname(FileInterface *) = 0;
    virtual ~FileFactory() = default;
};

} // namespace aare