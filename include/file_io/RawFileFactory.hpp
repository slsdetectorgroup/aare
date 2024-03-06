#include "file_io/File.hpp"
#include <filesystem>
template<DetectorType detector,typename DataType>  
class RawFileFactory{
    public:
        // RawFileFactory();
        // ~RawFileFactory();
        File<detector,DataType> loadFile(std::filesystem::path fpath);
};