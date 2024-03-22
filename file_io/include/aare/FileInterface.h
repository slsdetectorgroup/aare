#pragma once
#include "aare/Frame.hpp"
#include <filesystem>
namespace aare {
class FileInterface {
public:
    FileInterface(std::filesystem::path& fname ){}; //do we need this? 
    
    // read the frame number on position frame_index
    virtual size_t frame_number(size_t frame_index) = 0; 

    // read one frame
    virtual Frame read() = 0;

    // read n_frames frames
    virtual std::vector<Frame> read(size_t n_frames) = 0; //Is this the right interface?

    // read one frame into the provided buffer
    virtual void read_into(std::byte* image_buf) = 0;    

    // read n_frames frame into the provided buffer
    virtual void read_into(std::byte* image_buf, size_t n_frames) = 0; 

    // size of one frame, important fro teh read_into function
    virtual size_t bytes_per_frame() const = 0;
    
    // goto frame number
    virtual void seek(size_t frame_number) = 0;

    // return the position of the file pointer (in number of frames)
    virtual size_t tell() = 0;

    // total number of frames in the file
    virtual size_t total_frames() = 0;

    virtual size_t rows() const = 0;

    virtual size_t cols() const = 0;

    virtual ~FileInterface() = 0;
};
} // namespace aare
