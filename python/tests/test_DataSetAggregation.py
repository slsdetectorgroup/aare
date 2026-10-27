# SPDX-License-Identifier: MPL-2.0
import numpy as np
import pytest

from aare import block_mean, mean_current_source

def test_block_mean(create_temporary_jungfrau_files):
    """ test block_mean """
    rows, cols = 256, 256
    num_samples = 10
    num_frames_per_sample = 100
    num_processing_threads = 12
    num_reader_threads = 8

    file_system = create_temporary_jungfrau_files(
        num_samples * num_frames_per_sample,
        lambda frame: np.full((rows, cols), frame % num_frames_per_sample, dtype=np.uint16),
    )

    result = block_mean(file_system, num_samples = num_samples, mean_over_num_frames = num_frames_per_sample, num_processing_threads = num_processing_threads, num_reader_threads = num_reader_threads)

    assert result.shape == (rows, cols, num_samples)

    # mean of 0,1,...,num_frames_per_sample-1
    mean_value = (num_frames_per_sample - 1) / 2
    np.testing.assert_allclose(result, mean_value)


def test_mean_current_source(create_temporary_jungfrau_files):
    """ test mean_current_source """

    rows, cols = 256, 256
    columns_per_subcolumn = 64
    mean_over_num_frames = 3
    num_samples = 5
    file_offset = 2 * columns_per_subcolumn
    frames_per_sample = mean_over_num_frames * columns_per_subcolumn
    num_frames = num_samples * frames_per_sample + file_offset

    col_values = np.tile(np.arange(cols, dtype=np.uint16), (rows, 1))

    def frame_generator(frame):
        frame_idx = ((frame - file_offset) // columns_per_subcolumn) % mean_over_num_frames
        return col_values + frame_idx

    # spread over several files to cross file boundaries mid sample
    file_system = create_temporary_jungfrau_files(num_frames, frame_generator, num_files=3)

    result = mean_current_source(file_system, num_samples, mean_over_num_frames, 4, file_offset)

    assert result.shape == (rows, cols, num_samples)

    # mean of c + 0, c + 1, ..., c + mean_over_num_frames-1
    sum_mean_over_frames = mean_over_num_frames * (mean_over_num_frames - 1) / 2
    expected = (col_values * mean_over_num_frames + sum_mean_over_frames) / mean_over_num_frames
    np.testing.assert_allclose(result, np.repeat(expected[:, :, np.newaxis], num_samples, axis=2))
