#include "aare/RawFile.hpp"
#include "aare/utils/logger.hpp"

namespace aare {

Frame RawFile::get_frame(size_t frame_number) {
    auto f = Frame(this->m_rows, this->m_cols, this->m_bitdepth);
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_number, frame_buffer);
    return f;
}

void RawFile::get_frame_into(size_t frame_number, std::byte *frame_buffer) {
    if (frame_number > this->m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    int subfile_id = frame_number / this->max_frames_per_file;
    // create frame and get its buffer

    if (this->geometry.col == 1) {
        // get the part from each subfile and copy it to the frame
        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            auto part_offset = this->subfiles[subfile_id][part_idx]->bytes_per_part();
            this->subfiles[subfile_id][part_idx]->get_part(frame_buffer + part_idx * part_offset,
                                                           frame_number % this->max_frames_per_file);
        }

    } else {
        // create a buffer that will hold a the frame part
        auto bytes_per_part = this->subfile_rows * this->subfile_cols * this->m_bitdepth / 8;
        std::byte *part_buffer = new std::byte[bytes_per_part];

        for (size_t part_idx = 0; part_idx != this->n_subfile_parts; ++part_idx) {
            this->subfiles[subfile_id][part_idx]->get_part(part_buffer, frame_number % this->max_frames_per_file);
            for (int cur_row = 0; cur_row < (this->subfile_rows); cur_row++) {
                auto irow = cur_row + (part_idx / this->geometry.col) * this->subfile_rows;
                auto icol = (part_idx % this->geometry.col) * this->subfile_cols;
                auto dest = (irow * this->m_cols + icol);
                dest = dest * this->m_bitdepth / 8;
                memcpy(frame_buffer + dest, part_buffer + cur_row * this->subfile_cols * this->m_bitdepth / 8,
                       this->subfile_cols * this->m_bitdepth / 8);
            }
        }
        delete[] part_buffer;
    }
}

std::vector<Frame> RawFile::read(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(this->current_frame));
        this->current_frame++;
    }
    return frames;
}
void RawFile::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way
    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(this->current_frame++, image_buf);
        image_buf += this->bytes_per_frame();
    }
}

size_t RawFile::frame_number(size_t frame_index) {
    if (frame_index > this->m_total_frames) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    int subfile_id = frame_index / this->max_frames_per_file;
    return this->subfiles[subfile_id][0]->frame_number(frame_index % this->max_frames_per_file);
}

RawFile::~RawFile() {
    for (auto &vec : subfiles) {
        for (auto subfile : vec) {
            delete subfile;
        }
    }
}

} // namespace aare