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
    size_t m_first_frame_index{};

    std::vector<size_t> m_frames_in_file{};

    std::filesystem::path m_base_path;
    std::string m_base_name;

    //Data sizes to guess the frame size
    using value_type = uint16_t;
    static constexpr size_t header_size = sizeof(JungfrauDataHeader); 
    static constexpr size_t module_data_size = header_size + sizeof(value_type) * 512 * 1024; 
    static constexpr size_t half_data_size = header_size + sizeof(value_type) * 256 * 1024;
    static constexpr size_t chip_data_size = header_size + sizeof(value_type) * 256 * 256; 

    static constexpr size_t n_digits_in_file_index = 6;

  public:
    JungfrauDataFile(const std::filesystem::path &fname);

    std::string base_name() const; //!< get the base name of the file (without path and extension)
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
    static std::filesystem::path get_frame_path(const std::filesystem::path &path, const std::string& base_name, size_t frame_index);
};

} // namespace aare