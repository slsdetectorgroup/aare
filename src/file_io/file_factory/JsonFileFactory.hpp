#include "FileFactory.hpp"

class JsonFileFactory: public FileFactory
{
private:
    /* data */
public:
    File* load_file() override;
    void parse_metadata(File*) override;
    JsonFileFactory(std::filesystem::path fpath, uint16_t bitdepth);
    void open_subfiles(File*);
    


};

