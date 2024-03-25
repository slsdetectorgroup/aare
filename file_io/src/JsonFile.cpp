#include "aare/JsonFile.hpp"

Frame JsonFile::get_frame(size_t frame_number) {
    if (frame_number > this->total_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    std::byte *buffer = new std::byte[this->bytes_per_frame()];
    for (size_t i = 0; i != this->n_subfile_parts; ++i) {
        auto part_offset = this->subfiles[subfile_id][i]->bytes_per_part();
        this->subfiles[subfile_id][i]->get_part(buffer + i*part_offset, frame_number % this->max_frames_per_file);
    }
    auto f =  Frame(buffer, this->rows, this->cols, this->bitdepth );
    delete[] buffer;
    return f;
}

JsonFile::~JsonFile() {
    for (auto& vec : subfiles) {
        for (auto subfile : vec) {
            delete subfile;
        }
    }
}


