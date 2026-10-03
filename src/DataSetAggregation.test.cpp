#include "aare/DataSetAggregation.hpp"

#include "jungfrau_file_helpers.hpp"

#include <catch2/catch_test_macros.hpp>

#include <iostream>

using namespace aare;

TEST_CASE("mean_over_dataset", "[DataSetAggregation]") {

    // Create a temporary Jungfrau data file with 2200 frames, each frame is a

    const int num_samples = 100;
    const int num_frames_per_sample = 10;
    const int num_threads = 16;

    TemporaryJungfrauFiles temp_files(
        num_samples * num_frames_per_sample, [](size_t frame) {
            uint16_t value = frame % num_frames_per_sample;
            NDArray<uint16_t, 2> data({Jungfrau::nRows, Jungfrau::nCols},
                                      value);
            return data;
        });

    const std::filesystem::path path = temp_files.file_path();

    auto mean_result = mean_over_dataset<uint16_t>(
        path, num_samples, num_frames_per_sample, num_threads);

    // Check the shape of the result
    REQUIRE(mean_result.shape(0) == Jungfrau::nRows);
    REQUIRE(mean_result.shape(1) == Jungfrau::nCols);
    REQUIRE(mean_result.shape(2) == num_samples);

    uint16_t mean_value =
        (num_frames_per_sample - 1) /
        2; // ((num_frames_per_sample -1)*num_frames_per_sample) / (2
           // *num_frames_per_sample); // mean of
           // 0,1,...,num_frames_per_sample-1

    // Check that the mean values
    for (ssize_t sample = 0; sample < num_samples; ++sample) {
        for (ssize_t row = 0; row < mean_result.shape(0); ++row) {
            for (ssize_t col = 0; col < mean_result.shape(1); ++col) {
                if (mean_result(row, col, sample) != mean_value) {
                    std::cerr
                        << "Mismatch at sample: " << sample << ", row: " << row
                        << ", col: " << col << ". Expected: " << mean_value
                        << ", got: " << mean_result(row, col, sample) << "\n";
                }
                CHECK(mean_result(row, col, sample) == mean_value);
            }
        }
    }
}