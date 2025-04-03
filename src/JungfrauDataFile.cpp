#include "aare/JungfrauDataFile.hpp"
#include "aare/defs.hpp"

#include <fmt/format.h>

namespace aare{

JungfrauDataFile::JungfrauDataFile(const std::filesystem::path& fname){

    //setup geometry
    auto frame_size = guess_frame_size(fname);    
    if (frame_size == module_data_size) {
        m_rows = 512;
        m_cols = 1024;
    } else if (frame_size == half_data_size) {
        m_rows = 256;
        m_cols = 1024;
    } else if (frame_size == chip_data_size) {
        m_rows = 256;
        m_cols = 256;
    } else {
        throw std::runtime_error(LOCATION + "Cannot guess frame size: file size is not a multiple of any known frame size");
    }      

    m_base_path = fname.parent_path();
    m_base_name = fname.stem();

    //need to know the first 

    //remove digits
    while(std::isdigit(m_base_name.back())){
        m_base_name.pop_back();
    }

    //find how many files we have 
    // size_t frame_index = 0;
    // while (std::filesystem::exists(get_frame_path(m_base_path, m_base_name, frame_index))) {
    //     auto n_frames =
    //     m_frames_in_file.push_back(n_frames);
    //     ++frame_index;
    // }

}


std::string JungfrauDataFile::base_name() const {
    return m_base_name;
}

size_t JungfrauDataFile::bytes_per_frame() const{
    return m_rows * m_cols * bytes_per_pixel();
}          

size_t JungfrauDataFile::pixels_per_frame()const {
    return m_rows * m_cols;
}

size_t JungfrauDataFile::bytes_per_pixel() const {
    return 2;
}

size_t JungfrauDataFile::bitdepth()const {
    return 16;
}
void seek(size_t frame_index){};          //!< seek to the given frame index

size_t JungfrauDataFile::tell() const{
    return 0;
}                    //!< get the frame index of the file pointer
size_t JungfrauDataFile::total_frames() const {
    return m_total_frames;
}
size_t JungfrauDataFile::rows() const {
    return m_rows;
}
size_t JungfrauDataFile::cols() const {
    return m_cols;
}

size_t JungfrauDataFile::guess_frame_size(const std::filesystem::path& fname)  { 
    auto file_size = std::filesystem::file_size(fname);
    if (file_size == 0) {
        throw std::runtime_error(LOCATION + "Cannot guess frame size: file is empty");
    }

    

    if (file_size % module_data_size == 0) {
        return module_data_size;
    } else if (file_size % half_data_size == 0) {
        return half_data_size;
    } else if (file_size % chip_data_size == 0) {
        return chip_data_size;
    } else {
        throw std::runtime_error(LOCATION + "Cannot guess frame size: file size is not a multiple of any known frame size");
    }
}

std::filesystem::path JungfrauDataFile::get_frame_path(const std::filesystem::path& path, const std::string& base_name, size_t frame_index) {
    auto fname = fmt::format("{}{:0{}}.dat", base_name, frame_index, n_digits_in_file_index);
    return path / fname;
}

} // namespace aare