#pragma once

#include "aare/FileInterface.hpp"
#include "aare/Frame.hpp"
#include "aare/RawMasterFile.hpp"

#include <filesystem>
#include <fstream>

namespace aare {

class CtbRawFile {
    RawMasterFile m_master;
    std::ifstream m_file;
    size_t m_current_frame{0};
    size_t m_current_subfile{0};
    size_t m_num_subfiles{0};

  public:
    CtbRawFile(const std::filesystem::path &fname);

    void read_into(std::byte *image_buf, DetectorHeader *header = nullptr);
    void seek(size_t frame_index); //!< seek to the given frame index
    size_t tell() const;           //!< get the frame index of the file pointer

    // in the specific class we can expose more functionality

    size_t image_size_in_bytes() const;
    size_t frames_in_file() const;

    RawMasterFile master() const;

  private:
    void find_subfiles();
    size_t sub_file_index(size_t frame_index) const {
        return frame_index / m_master.max_frames_per_file();
    }
    void open_data_file(size_t subfile_index);
};

} // namespace aare