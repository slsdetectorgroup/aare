#include "File.hpp"
#include <filesystem>

class RawFileFactory{
    public:
        // RawFileFactory();
        // ~RawFileFactory();
        File loadFile(std::filesystem::path fpath);
};