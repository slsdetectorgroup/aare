#include "aare/FileInterface.hpp"
class File : public FileInterface {
  private:
    FileInterface *file_impl;

  public:
    // options:
    //  - r reading
    //  - w writing (overwrites existing file)
    //  - a appending (appends to existing file)
    // TODO! do we need to support w+, r+ and a+?
    File(std::filesystem::path fname, std::string mode);
    Frame read() override;
    std::vector<Frame> read(size_t n_frames) override;
    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override;
    size_t bytes_per_frame() override;
    size_t pixels() override;
    void seek(size_t frame_number) override;
    size_t tell() override;
    size_t total_frames() const override ;
    ssize_t rows() const override ;
    ssize_t cols() const  override ;
    ssize_t bitdepth()  const override ;
    File(File &&other);


    ~File();
};
