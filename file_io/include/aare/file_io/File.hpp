#pragma once
#include "aare/file_io/FileInterface.hpp"

namespace aare {
class File {
  private:
    FileInterface *file_impl;

  public:
    // options:
    //  - r reading
    //  - w writing (overwrites existing file)
    //  - a appending (appends to existing file)
    // TODO! do we need to support w+, r+ and a+?
    File(std::filesystem::path fname, std::string mode, FileConfig cfg = {});
    void write(Frame &frame);
    Frame read();
    Frame iread(size_t frame_number);
    std::vector<Frame> read(size_t n_frames);
    void read_into(std::byte *image_buf);
    void read_into(std::byte *image_buf, size_t n_frames);
    size_t frame_number(size_t frame_index);
    size_t bytes_per_frame();
    size_t pixels();
    void seek(size_t frame_number);
    size_t tell() const;
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;
    size_t bitdepth() const;
    File(File &&other);

    ~File();
};

} // namespace aare