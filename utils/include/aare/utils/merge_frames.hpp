#pragma once
#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include <cassert>
#include <cstring>
#include <vector>

namespace aare {
void merge_frames(std::vector<std::byte *> &part_buffers, size_t part_size, std::byte *merged_frame, const xy &geometry,
                  size_t rows = 0, size_t cols = 0, size_t bitdepth = 0) {

    assert(part_buffers.size() == geometry.row * geometry.col);

    if (geometry.col == 1) {
        // get the part from each subfile and copy it to the frame
        size_t part_idx = 0;
        for (auto part_buffer : part_buffers) {
            memcpy(merged_frame + part_idx * part_size, part_buffer, part_size);
            part_idx++;
        }

    } else {
        std::cout << "cols: " << cols << " rows: " << rows << " bitdepth: " << bitdepth << std::endl;
        assert(cols != 0 && rows != 0 && bitdepth != 0);
        size_t part_rows = rows / geometry.row;
        size_t part_cols = cols / geometry.col;
        // create a buffer that will hold a the frame part
        size_t part_idx = 0;
        for (auto part_buffer : part_buffers) {
            for (size_t cur_row = 0; cur_row < (part_rows); cur_row++) {
                auto irow = cur_row + (part_idx / geometry.col) * part_rows;
                auto icol = (part_idx % geometry.col) * part_cols;
                auto dest = (irow * cols + icol);
                dest = dest * bitdepth / 8;
                memcpy(merged_frame + dest, part_buffer + cur_row * part_cols * bitdepth / 8, part_cols * bitdepth / 8);
            }
            part_idx++;
        }
    }
}
} // namespace aare