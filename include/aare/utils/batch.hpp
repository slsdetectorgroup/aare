// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <vector>
#include <cstring>
#include "aare/NDArray.hpp"


template <typename FRAME_TYPE>
void pack_frame_batch(const std::vector<aare::NDArray<FRAME_TYPE, 2>>& frames,
                      size_t first_frame,
                      size_t n_frames,
                      std::vector<FRAME_TYPE>& batch)
{
    if (n_frames == 0) return;

    const size_t rows = frames[first_frame].shape(0);
    const size_t cols = frames[first_frame].shape(1);
    const size_t image_size = rows * cols;
    const size_t total_size = n_frames * image_size;

    if (batch.size() != total_size) {
        batch.resize(total_size);
    }

    for (size_t k = 0; k < n_frames; ++k) {
        const FRAME_TYPE* src = frames[first_frame + k].data();
        FRAME_TYPE* dst = batch.data() + k * image_size;
        std::memcpy(dst, src, image_size * sizeof(FRAME_TYPE));
    }
}