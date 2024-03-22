#include "aare/FileFactory.hpp"
#include "aare/JsonFile.hpp"
class JsonFileFactory: public FileFactory
{
private:
    /* data */
public:
    JsonFileFactory(std::filesystem::path fpath);
    virtual JsonFile& load_file() override;
    void parse_metadata(File*) override;
    void parse_fname(File*) override;
    void open_subfiles(File*);
    sls_detector_header read_header(const std::filesystem::path &fname);
    void find_geometry(File*);



};
