#pragma once
#include <cstdint>
#include <filesystem>
namespace aare {

struct JungfrauDataHeader{
    uint64_t framenum;
    uint64_t bunchid;
};

class JungfrauDataFile {

    size_t m_rows{};
    size_t m_cols{};
    size_t m_total_frames{};

    //Data sizes to guess the frame size
    using value_type = uint16_t;
    static constexpr size_t header_size = sizeof(JungfrauDataHeader); 
    static constexpr size_t module_data_size = header_size + sizeof(value_type) * 512 * 1024; 
    static constexpr size_t half_data_size = header_size + sizeof(value_type) * 256 * 1024;
    static constexpr size_t chip_data_size = header_size + sizeof(value_type) * 256 * 256; 

  public:
    JungfrauDataFile(const std::filesystem::path &fname);
    size_t bytes_per_frame() const;           
    size_t pixels_per_frame() const;    
    size_t bytes_per_pixel() const;  
    size_t bitdepth() const;
    void seek(size_t frame_index);          //!< seek to the given frame index
    size_t tell() const;                     //!< get the frame index of the file pointer
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;

    void read_into(std::byte *image_buf, JungfrauDataHeader *header);

    static size_t guess_frame_size(const std::filesystem::path &fname);
};

} // namespace aare