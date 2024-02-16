#include "FileFactory.hpp"

class JsonFileFactory: public FileFactory
{
private:
    /* data */
public:
    File loadFile() override;
    void parse_metadata(File&) override;
    JsonFileFactory(std::filesystem::path fpath);


};

