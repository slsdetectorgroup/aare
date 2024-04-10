#pragma once
#include "aare/file_io/RawFile.hpp"


namespace aare::RawFileHelpers {
    void parse_json_metadata(RawFile *file);
    void parse_raw_metadata(RawFile *file);
    RawFile *load_file_read() override;
    RawFile *load_file_write(FileConfig) override { return new RawFile(); };
    void parse_metadata(FileInterface *) override;
    void parse_fname(FileInterface *) override;
    void open_subfiles(FileInterface *);
    sls_detector_header read_header(const std::filesystem::path &fname);
    void find_geometry(FileInterface *);
};

