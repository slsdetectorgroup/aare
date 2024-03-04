#include "JsonFile.hpp"
#include <typeinfo>

FrameImpl* JsonFile::get_frame(int frame_number){
    int subfile_id=frame_number/max_frames_per_file;
    std::byte* buffer;
    size_t frame_size = subfiles[subfile_id]->bytes_per_frame();
    buffer = new std::byte[frame_size];

    subfiles[subfile_id]->get_frame(buffer, frame_number%max_frames_per_file);



    auto f = new FrameImpl(buffer, rows, cols, bitdepth);

    delete[] buffer;
    return f;

    



}