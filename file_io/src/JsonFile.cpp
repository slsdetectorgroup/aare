#include "aare/JsonFile.hpp"

Frame *JsonFile::get_frame(size_t frame_number) {
    if (frame_number > this->total_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    std::byte *buffer;
    size_t frame_size = this->subfiles[subfile_id]->bytes_per_frame();
    buffer = new std::byte[frame_size];
    this->subfiles[subfile_id]->get_frame(buffer, frame_number % this->max_frames_per_file);
    auto f = new Frame(buffer, this->rows, this->cols, this->bitdepth );


    delete[] buffer;
    return f;
}

JsonFile::~JsonFile() {
    for (auto& subfile : subfiles) {
        delete subfile;
    }
}


