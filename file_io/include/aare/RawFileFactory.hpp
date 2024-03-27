#include "aare/FileFactory.hpp"
#include "aare/RawFile.hpp"
class RawFileFactory : public FileFactory {
  private:
    void parse_json_metadata(RawFile *file);
    void parse_raw_metadata(RawFile *file);

  public:
    RawFileFactory(std::filesystem::path fpath);
    virtual RawFile *load_file() override;
    void parse_metadata(FileInterface *) override;
    void parse_fname(FileInterface *) override;
    void open_subfiles(FileInterface *);
    sls_detector_header read_header(const std::filesystem::path &fname);
    void find_geometry(FileInterface *);
};
