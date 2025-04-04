#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "aare/FilePtr.hpp"
#include "aare/defs.hpp"
#include "aare/NDArray.hpp"
namespace aare {

struct JungfrauDataHeader{
    uint64_t framenum;
    uint64_t bunchid;
};

class JungfrauDataFile {

    size_t m_rows{};
    size_t m_cols{};
    size_t m_bytes_per_frame{};
    size_t m_total_frames{};
    size_t m_offset{}; // file index of the first file, allow starting at non zero file
    size_t m_current_file_index{};
    size_t m_current_frame{};

    std::vector<size_t> m_frame_index{};

    std::filesystem::path m_path;
    std::string m_base_name;

    FilePtr m_fp;

    //Data sizes to guess the frame size
    using pixel_type = uint16_t;
    static constexpr size_t header_size = sizeof(JungfrauDataHeader); 
    static constexpr size_t module_data_size = header_size + sizeof(pixel_type) * 512 * 1024; 
    static constexpr size_t half_data_size = header_size + sizeof(pixel_type) * 256 * 1024;
    static constexpr size_t chip_data_size = header_size + sizeof(pixel_type) * 256 * 256; 
    static constexpr size_t n_digits_in_file_index = 6; // number of digits in the file index

  public:
    JungfrauDataFile(const std::filesystem::path &fname);

    std::string base_name() const;      //!< get the base name of the file (without path and extension)
    size_t bytes_per_frame() const;           
    size_t pixels_per_frame() const;    
    size_t bytes_per_pixel() const;  
    size_t bitdepth() const;
    void seek(size_t frame_index);          //!< seek to the given frame index
    size_t tell() const;                     //!< get the frame index of the file pointer
    size_t total_frames() const;
    size_t rows() const;
    size_t cols() const;
    size_t n_files() const { return m_frame_index.size(); } //!< get the number of files

    void read_into(std::byte *image_buf, JungfrauDataHeader *header);
    void read_into(std::byte *image_buf, size_t n_frames, JungfrauDataHeader *header);
    void read_into(NDArray<uint16_t> &image, JungfrauDataHeader &header) {
        if(!(rows() == image.shape(0) && cols() == image.shape(1))){
            throw std::runtime_error(LOCATION +
                "Image shape does not match file size: " + std::to_string(rows()) + "x" + std::to_string(cols()));
        }
        read_into(reinterpret_cast<std::byte *>(image.data()), &header);
    }
    NDArray<uint16_t> read_frame(JungfrauDataHeader& header) {
        Shape<2> shape{rows(), cols()};
        NDArray<uint16_t> image(shape);

        read_into(reinterpret_cast<std::byte *>(image.data()),
                &header);

        return image;
    }

    std::filesystem::path current_file() const { return fpath(m_current_file_index+m_offset); }

    
    private:
     void find_frame_size(const std::filesystem::path &fname);
     void parse_fname(const std::filesystem::path &fname);
     void scan_files();
     void open_file(size_t file_index);
     std::filesystem::path fpath(size_t frame_index) const;


  };

} // namespace aare