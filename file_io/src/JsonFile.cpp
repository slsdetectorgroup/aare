#include "aare/JsonFile.hpp"
#include "aare/utils/logger.hpp"

Frame JsonFile::get_frame(size_t frame_number) {
    if (frame_number > this->total_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    // create frame and get its buffer
    auto f = Frame(this->rows, this->cols, this->bitdepth);
    std::byte *frame_buffer = f._get_data();

    if (this->geometry.col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto part_offset = this->subfiles[subfile_id][part_idx]->bytes_per_part();
            this->subfiles[subfile_id][part_idx]->get_part(frame_buffer + part_idx*part_offset, frame_number % this->max_frames_per_file);
        }

    } else {
        // create a buffer that will hold a the frame part
        auto bytes_per_part = this->subfile_rows * this->subfile_cols * this->bitdepth / 8;
        std::byte *part_buffer = new std::byte[bytes_per_part];

        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            this->subfiles[subfile_id][part_idx]->get_part(part_buffer, frame_number % this->max_frames_per_file);
            for (int cur_row = 0; cur_row < (this->subfile_rows); cur_row++) {
                auto irow = cur_row + (part_idx / this->geometry.col) * this->subfile_rows;
                auto icol = (part_idx % this->geometry.col) * this->subfile_cols;
                auto dest = (irow * this->cols + icol);
                dest = dest * this->bitdepth / 8;
                memcpy(frame_buffer + dest, part_buffer + cur_row * this->subfile_cols * this->bitdepth / 8,
                       this->subfile_cols * this->bitdepth / 8);
            }
        }
        delete[] part_buffer;
    }

    return f;
}

JsonFile::~JsonFile() {
    for (auto &vec : subfiles) {
        for (auto subfile : vec) {
            delete subfile;
        }
    }
}
