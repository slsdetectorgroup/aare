#pragma once
#include "aare/FileInterface.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp" //for pixel map
#include "aare/RawSubFile.hpp"


#include <optional>

namespace aare {

struct ModuleConfig {
    int module_gap_row{};
    int module_gap_col{};

    bool operator==(const ModuleConfig &other) const {
        if (module_gap_col != other.module_gap_col)
            return false;
        if (module_gap_row != other.module_gap_row)
            return false;
        return true;
    }
};

/**
 * @brief Class to read .raw files. The class will parse the master file
 * to find the correct geometry for the frames.
 * @note A more generic interface is available in the aare::File class.
 * Consider using that unless you need raw file specific functionality.
 */
class RawFile : public FileInterface {
    size_t n_subfiles{}; //f0,f1...fn
    size_t n_subfile_parts{}; // d0,d1...dn
    //TODO! move to vector of SubFile instead of pointers
    std::vector<std::vector<RawSubFile *>> subfiles; //subfiles[f0,f1...fn][d0,d1...dn]
    // std::vector<xy> positions;
    
    ModuleConfig cfg{0, 0};

    RawMasterFile m_master;

    size_t m_current_frame{};

    // std::vector<ModuleGeometry> m_module_pixel_0;
    // size_t m_rows{};
    // size_t m_cols{};

    DetectorGeometry m_geometry;

  public:
    /**
     * @brief RawFile constructor
     * @param fname path to the master file (.json)
     * @param mode file mode (only "r" is supported at the moment)

     */
    RawFile(const std::filesystem::path &fname, const std::string &mode = "r");
    virtual ~RawFile() override;

    Frame read_frame() override;
    Frame read_frame(size_t frame_number) override;
    std::vector<Frame> read_n(size_t n_frames) override;
    void read_into(std::byte *image_buf) override;
    void read_into(std::byte *image_buf, size_t n_frames) override;

    //TODO! do we need to adapt the API? 
    void read_into(std::byte *image_buf, DetectorHeader *header);
    void read_into(std::byte *image_buf, size_t n_frames, DetectorHeader *header);


    size_t frame_number(size_t frame_index) override;
    size_t bytes_per_frame() override;
    size_t pixels_per_frame() override;
    size_t bytes_per_pixel() const;
    void seek(size_t frame_index) override;
    size_t tell() override;
    size_t total_frames() const override;
    size_t rows() const override;
    size_t cols() const override;
    size_t bitdepth() const override;
    xy geometry();
    size_t n_mod() const;
    
    RawMasterFile master() const;



    DetectorType detector_type() const override;

  private:

    /**
     * @brief read the frame at the given frame index into the image buffer
     * @param frame_number frame number to read
     * @param image_buf buffer to store the frame
     */

    void get_frame_into(size_t frame_index, std::byte *frame_buffer, DetectorHeader *header = nullptr);


    /**
     * @brief get the frame at the given frame index
     * @param frame_number frame number to read
     * @return Frame
     */
    Frame get_frame(size_t frame_index);



    /**
     * @brief read the header of the file
     * @param fname path to the data subfile
     * @return DetectorHeader
     */
    static DetectorHeader read_header(const std::filesystem::path &fname);

    // void update_geometry_with_roi();
    int find_number_of_subfiles();

    void open_subfiles();
    void find_geometry();
};


} // namespace aare