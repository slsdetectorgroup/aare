#include "aare/File.hpp"
#include "aare/FileFactory.hpp"
#include <filesystem>
#include <memory>


/**
 * @brief A class to manage the context of the files and network connections
 * closes the files and network connections when the context is destroyed
*/
class ContextManager {
  public:
    ContextManager() = default;

    File *get_file(std::filesystem::path fname) {
        auto tmp_file = FileFactory::load_file(fname);
        this->files.push_back(tmp_file);
        return tmp_file;
    }

    // prevent default copying, it can delete the file twice
    ContextManager(const ContextManager &) = delete;
    ContextManager &operator=(const ContextManager &) = delete;

    ~ContextManager() {
        for (auto f : files) {
            delete f;
        }
    }

  private:
    std::vector<File *> files;
    // std::vector<NetworkConnection*> connections;
};