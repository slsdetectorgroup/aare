#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

#include "aare/FilePtr.hpp"
#include "aare/defs.hpp"
#include "aare/NDArray.hpp"
#include "aare/FileInterface.hpp"
namespace aare {


struct JungfrauDataHeader{
    uint64_t framenum;
    uint64_t bunchid;
};

class JungfrauDataFile : public FileInterface {

    size_t m_rows{};     //!< number of rows in the image, from find_frame_size();
    size_t m_cols{};     //!< number of columns in the image, from find_frame_size();
    size_t m_bytes_per_frame{}; //!< number of bytes per frame excluding header
    size_t m_total_frames{};    //!< total number of frames in the series of files
    size_t m_offset{}; //!< file index of the first file, allow starting at non zero file
    size_t m_current_file_index{};   //!< The index of the open file
    size_t m_current_frame_index{};  //!< The index of the current frame (with reference to all files)

    std::vector<size_t> m_last_frame_in_file{}; //!< Used for seeking to the correct file
    std::filesystem::path m_path; //!< path to the files
    std::string m_base_name;      //!< base name used for formatting file names

    FilePtr m_fp; //!< RAII wrapper for a FILE* 

    
    using pixel_type = uint16_t;
    static constexpr size_t header_size = sizeof(JungfrauDataHeader); 
    static constexpr size_t n_digits_in_file_index = 6; //!< to format file names

  public:
    JungfrauDataFile(const std::filesystem::path &fname);

    std::string base_name() const;      //!< get the base name of the file (without path and extension)
    size_t bytes_per_frame() override;           
    size_t pixels_per_frame() override;    
    size_t bytes_per_pixel() const;  
    size_t bitdepth() const override;
    void seek(size_t frame_index) override;  //!< seek to the given frame index (note not byte offset)
    size_t tell() override;            //!< get the frame index of the file pointer
    size_t total_frames() const override;
    size_t rows() const override;
    size_t cols() const override;
    std::array<ssize_t,2> shape() const;
    size_t n_files() const; //!< get the number of files in the series. 

    // Extra functions needed for FileInterface
    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames=0) override;
    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;
    size_t frame_number(size_t frame_index) override;
    DetectorType detector_type() const override;

    /**
     * @brief Read a single frame from the file into the given buffer.  
     * @param image_buf buffer to read the frame into. (Note the caller is responsible for allocating the buffer)
     * @param header pointer to a JungfrauDataHeader or nullptr to skip header)
     */
    void read_into(std::byte *image_buf, JungfrauDataHeader *header = nullptr);

    /**
     * @brief Read a multiple frames from the file into the given buffer.  
     * @param image_buf buffer to read the frame into. (Note the caller is responsible for allocating the buffer)
     * @param n_frames number of frames to read
     * @param header pointer to a JungfrauDataHeader or nullptr to skip header)
     */
    void read_into(std::byte *image_buf, size_t n_frames, JungfrauDataHeader *header = nullptr);
    
    /** 
     * @brief Read a single frame from the file into the given NDArray
     * @param image NDArray to read the frame into.
     */
    void read_into(NDArray<uint16_t>* image, JungfrauDataHeader* header = nullptr);

    JungfrauDataHeader read_header();
    std::filesystem::path current_file() const { return fpath(m_current_file_index+m_offset); }

    
    private:
    /**
     * @brief Find the size of the frame in the file. (256x256, 256x1024, 512x1024)
     * @param fname path to the file
     * @throws std::runtime_error if the file is empty or the size cannot be determined
     */
     void find_frame_size(const std::filesystem::path &fname);


     void parse_fname(const std::filesystem::path &fname);
     void scan_files();
     void open_file(size_t file_index);
     std::filesystem::path fpath(size_t frame_index) const;


  };

} // namespace aare