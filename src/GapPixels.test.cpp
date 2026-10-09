// SPDX-License-Identifier: MPL-2.0
#include "aare/GapPixels.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <numeric>

using namespace aare;

namespace {

// chip 2, module 4, chip gap 1, module gap 3: boundaries at 2 (chip), 4
// (module), 6 (chip) for an 8 pixel axis
const detail::AxisGaps tiny_axis{2, 4, 1, 3};

template <typename T> NDArray<T, 2> pattern(ssize_t rows, ssize_t cols) {
    NDArray<T, 2> image({rows, cols});
    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            image(row, col) = static_cast<T>((row * cols + col) % 60000 + 1);
        }
    }
    return image;
}

template <typename T> double total(NDView<T, 2> image) {
    double sum = 0;
    for (ssize_t row = 0; row < image.shape(0); ++row) {
        for (ssize_t col = 0; col < image.shape(1); ++col) {
            sum += static_cast<double>(image(row, col));
        }
    }
    return sum;
}

} // namespace

TEST_CASE("Axis gap map inserts chip and module gaps", "[GapPixels]") {
    const detail::AxisGaps &axis = tiny_axis;
    CHECK(axis.gapped(0) == 0);
    CHECK(axis.gapped(1) == 1);
    CHECK(axis.gapped(2) == 3);
    CHECK(axis.gapped(3) == 4);
    CHECK(axis.gapped(4) == 8);
    CHECK(axis.gapped(5) == 9);
    CHECK(axis.gapped(6) == 11);
    CHECK(axis.gapped(7) == 12);

    CHECK(axis.extent(0, 8) == 13);
    CHECK(axis.extent(0, 2) == 2);
    CHECK(axis.extent(2, 4) == 2);
    CHECK(axis.extent(1, 3) == 3);
    CHECK(axis.extent(3, 5) == 5);
    CHECK(axis.extent(4, 4) == 0);
    CHECK(axis.extent(5, 4) == 0);

    CHECK(axis.inner_boundaries(0, 8) == std::vector<ssize_t>{2, 4, 6});
    CHECK(axis.inner_boundaries(1, 3) == std::vector<ssize_t>{2});
    CHECK(axis.inner_boundaries(2, 4).empty());
    CHECK(axis.inner_boundaries(2, 5) == std::vector<ssize_t>{4});
    CHECK(axis.inner_boundaries(0, 0).empty());

    CHECK(axis.is_module_boundary(4));
    CHECK_FALSE(axis.is_module_boundary(2));
    CHECK(axis.gap_range(2, 0) == std::pair<ssize_t, ssize_t>{2, 2});
    CHECK(axis.gap_range(4, 0) == std::pair<ssize_t, ssize_t>{5, 7});
    CHECK(axis.gap_range(4, 3) == std::pair<ssize_t, ssize_t>{1, 3});
}

TEST_CASE("Gap layout follows the detector type", "[GapPixels]") {
    SECTION("Jungfrau defaults") {
        const auto layout =
            detail::gap_layout(DetectorType::Jungfrau, false, GapPixels{});
        CHECK(layout.x.chip == 256);
        CHECK(layout.x.module == 1024);
        CHECK(layout.x.chip_gap == 2);
        CHECK(layout.x.module_gap == 0);
        CHECK(layout.y.chip == 256);
        CHECK(layout.y.module == 512);
        CHECK(layout.y.chip_gap == 2);
        CHECK(layout.y.module_gap == 0);
    }
    SECTION("Eiger quad has a 512 pixel wide module") {
        CHECK(detail::gap_layout(DetectorType::Eiger, true, GapPixels{})
                  .x.module == 512);
        CHECK(detail::gap_layout(DetectorType::Eiger, false, GapPixels{})
                  .x.module == 1024);
    }
    SECTION("explicit gaps") {
        GapPixels gaps;
        gaps.chip_gap = 4;
        gaps.module_gaps = ModuleGaps{8, 36};
        const auto layout =
            detail::gap_layout(DetectorType::Jungfrau, false, gaps);
        CHECK(layout.x.chip_gap == 4);
        CHECK(layout.y.chip_gap == 4);
        CHECK(layout.x.module_gap == 8);
        CHECK(layout.y.module_gap == 36);
    }
    SECTION("unsupported detectors throw") {
        for (auto detector :
             {DetectorType::Moench, DetectorType::Mythen3,
              DetectorType::Gotthard2, DetectorType::ChipTestBoard,
              DetectorType::Generic, DetectorType::Unknown}) {
            CHECK_THROWS_AS(detail::gap_layout(detector, false, GapPixels{}),
                            std::invalid_argument);
        }
    }
    SECTION("invalid configuration throws") {
        GapPixels gaps;
        gaps.chip_gap = -1;
        CHECK_THROWS_AS(detail::gap_layout(DetectorType::Jungfrau, false, gaps),
                        std::invalid_argument);
        gaps = GapPixels{};
        gaps.module_gaps = ModuleGaps{-8, 36};
        CHECK_THROWS_AS(detail::gap_layout(DetectorType::Eiger, false, gaps),
                        std::invalid_argument);
        gaps = GapPixels{};
        gaps.split_counts = true;
        gaps.chip_gap = 4;
        CHECK_THROWS_AS(detail::gap_layout(DetectorType::Jungfrau, false, gaps),
                        std::invalid_argument);
    }
}

TEST_CASE("Gapped shape of detector ROIs", "[GapPixels]") {
    GapPixels module_8_36;
    module_8_36.module_gaps = ModuleGaps{8, 36};
    using Shape = std::array<ssize_t, 2>;

    CHECK(gapped_shape(ROI{0, 1024, 0, 512}, DetectorType::Jungfrau, false) ==
          Shape{514, 1030});
    CHECK(gapped_shape(ROI{0, 1024, 0, 512}, DetectorType::Jungfrau, false,
                       module_8_36) == Shape{514, 1030});
    CHECK(gapped_shape(ROI{0, 2048, 0, 512}, DetectorType::Jungfrau, false) ==
          Shape{514, 2060});
    CHECK(gapped_shape(ROI{0, 2048, 0, 512}, DetectorType::Jungfrau, false,
                       module_8_36) == Shape{514, 2068});
    CHECK(gapped_shape(ROI{0, 1024, 0, 1024}, DetectorType::Jungfrau, false) ==
          Shape{1028, 1030});
    CHECK(gapped_shape(ROI{0, 1024, 0, 1024}, DetectorType::Jungfrau, false,
                       module_8_36) == Shape{1064, 1030});
    CHECK(gapped_shape(ROI{0, 512, 0, 512}, DetectorType::Eiger, true) ==
          Shape{514, 514});
    CHECK(gapped_shape(ROI{0, 1024, 0, 512}, DetectorType::Eiger, false) ==
          Shape{514, 1030});

    SECTION("ROI borders exclude gaps") {
        CHECK(gapped_shape(ROI{100, 400, 0, 10}, DetectorType::Jungfrau,
                           false) == Shape{10, 302});
        CHECK(gapped_shape(ROI{256, 512, 0, 1}, DetectorType::Jungfrau,
                           false) == Shape{1, 256});
        CHECK(gapped_shape(ROI{255, 257, 0, 1}, DetectorType::Jungfrau,
                           false) == Shape{1, 4});
        CHECK(gapped_shape(ROI{200, 301, 300, 601}, DetectorType::Jungfrau,
                           false) == Shape{301, 103});
        CHECK(gapped_shape(ROI{200, 301, 300, 601}, DetectorType::Jungfrau,
                           false, module_8_36) == Shape{337, 103});
        CHECK(gapped_shape(ROI{512, 768, 0, 256}, DetectorType::Jungfrau,
                           false) == Shape{256, 256});
    }
    SECTION("invalid ROI throws") {
        CHECK_THROWS_AS(
            gapped_shape(ROI{-1, 10, 0, 10}, DetectorType::Jungfrau, false),
            std::invalid_argument);
        CHECK_THROWS_AS(
            gapped_shape(ROI{10, 5, 0, 10}, DetectorType::Jungfrau, false),
            std::invalid_argument);
    }
}

TEST_CASE("Copy and fill with a tiny layout", "[GapPixels]") {
    const detail::GapLayout layout{tiny_axis, tiny_axis};
    // 4 rows (one chip boundary at 2) by 8 columns (chip, module, chip)
    const ROI roi{0, 8, 0, 4};
    auto source = pattern<uint16_t>(4, 8);
    const auto shape = detail::gapped_shape(roi, layout);
    REQUIRE(shape == std::array<ssize_t, 2>{5, 13});

    NDArray<uint16_t, 2> destination(shape, uint16_t{7});
    detail::fill_gap_pixels(destination.view(), roi, layout, uint16_t{99});

    SECTION("whole ROI") {
        detail::copy_with_gaps(NDView<const uint16_t, 2>(source.view()), roi,
                               destination.view(), roi, layout);
        for (ssize_t row = 0; row < shape[0]; ++row) {
            for (ssize_t col = 0; col < shape[1]; ++col) {
                CAPTURE(row, col);
                const bool gap_row = row == 2;
                const bool gap_col =
                    col == 2 || (col >= 5 && col <= 7) || col == 10;
                if (gap_row || gap_col) {
                    CHECK(destination(row, col) == 99);
                }
            }
        }
        for (ssize_t row = 0; row < 4; ++row) {
            for (ssize_t col = 0; col < 8; ++col) {
                CAPTURE(row, col);
                CHECK(destination(layout.y.gapped(row), layout.x.gapped(col)) ==
                      source(row, col));
            }
        }
    }
    SECTION("two rectangles cover the ROI") {
        const ROI left{0, 3, 0, 4};
        const ROI right{3, 8, 0, 4};
        NDArray<uint16_t, 2> left_pixels({4, 3});
        NDArray<uint16_t, 2> right_pixels({4, 5});
        for (ssize_t row = 0; row < 4; ++row) {
            for (ssize_t col = 0; col < 8; ++col) {
                if (col < 3) {
                    left_pixels(row, col) = source(row, col);
                } else {
                    right_pixels(row, col - 3) = source(row, col);
                }
            }
        }
        detail::copy_with_gaps(NDView<const uint16_t, 2>(left_pixels.view()),
                               left, destination.view(), roi, layout);
        detail::copy_with_gaps(NDView<const uint16_t, 2>(right_pixels.view()),
                               right, destination.view(), roi, layout);
        for (ssize_t row = 0; row < 4; ++row) {
            for (ssize_t col = 0; col < 8; ++col) {
                CAPTURE(row, col);
                CHECK(destination(layout.y.gapped(row), layout.x.gapped(col)) ==
                      source(row, col));
            }
        }
        CHECK(destination(0, 2) == 99);
        CHECK(destination(2, 0) == 99);
    }
    SECTION("mismatched shapes throw") {
        NDArray<uint16_t, 2> wrong({5, 12});
        CHECK_THROWS_AS(
            detail::fill_gap_pixels(wrong.view(), roi, layout, uint16_t{0}),
            std::invalid_argument);
        CHECK_THROWS_AS(
            detail::copy_with_gaps(NDView<const uint16_t, 2>(source.view()),
                                   roi, wrong.view(), roi, layout),
            std::invalid_argument);
        CHECK_THROWS_AS(detail::copy_with_gaps(
                            NDView<const uint16_t, 2>(source.view()),
                            ROI{0, 9, 0, 4}, destination.view(), roi, layout),
                        std::invalid_argument);
        CHECK_THROWS_AS(detail::copy_with_gaps(
                            NDView<const uint16_t, 2>(source.view()),
                            ROI{0, 7, 0, 4}, destination.view(), roi, layout),
                        std::invalid_argument);
    }
}

TEST_CASE("insert_gap_pixels on a Jungfrau module", "[GapPixels]") {
    const ROI roi{0, 1024, 0, 512};
    auto source = pattern<uint16_t>(512, 1024);
    GapPixels gaps;
    gaps.fill_value = 5;
    auto result = insert_gap_pixels(NDView<const uint16_t, 2>(source.view()),
                                    roi, DetectorType::Jungfrau, false, gaps);
    REQUIRE(result.shape() == std::array<ssize_t, 2>{514, 1030});

    const auto layout = detail::gap_layout(DetectorType::Jungfrau, false, gaps);
    for (ssize_t row = 0; row < 512; ++row) {
        for (ssize_t col = 0; col < 1024; ++col) {
            if (result(layout.y.gapped(row), layout.x.gapped(col)) !=
                source(row, col)) {
                CAPTURE(row, col);
                REQUIRE(result(layout.y.gapped(row), layout.x.gapped(col)) ==
                        source(row, col));
            }
        }
    }
    for (ssize_t col : {256, 257, 514, 515, 772, 773}) {
        for (ssize_t row = 0; row < 514; ++row) {
            if (result(row, col) != 5) {
                CAPTURE(row, col);
                REQUIRE(result(row, col) == 5);
            }
        }
    }
    for (ssize_t row : {256, 257}) {
        for (ssize_t col = 0; col < 1030; ++col) {
            if (result(row, col) != 5) {
                CAPTURE(row, col);
                REQUIRE(result(row, col) == 5);
            }
        }
    }
    CHECK(total(NDView<const uint16_t, 2>(result.view())) ==
          total(NDView<const uint16_t, 2>(source.view())) +
              5.0 * (514 * 1030 - 512 * 1024));
}

TEST_CASE("insert_gap_pixels on ROIs", "[GapPixels]") {
    SECTION("gap inside the ROI") {
        const ROI roi{255, 257, 10, 11};
        NDArray<uint32_t, 2> source({1, 2});
        source(0, 0) = 11;
        source(0, 1) = 22;
        auto result =
            insert_gap_pixels(NDView<const uint32_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{1, 4});
        CHECK(result(0, 0) == 11);
        CHECK(result(0, 1) == 0);
        CHECK(result(0, 2) == 0);
        CHECK(result(0, 3) == 22);
    }
    SECTION("gap on the ROI border is not inserted") {
        const ROI roi{256, 260, 0, 2};
        auto source = pattern<uint8_t>(2, 4);
        auto result = insert_gap_pixels(NDView<const uint8_t, 2>(source.view()),
                                        roi, DetectorType::Jungfrau, false);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{2, 4});
        for (ssize_t row = 0; row < 2; ++row) {
            for (ssize_t col = 0; col < 4; ++col) {
                CHECK(result(row, col) == source(row, col));
            }
        }
    }
    SECTION("module boundary inside the ROI") {
        GapPixels gaps;
        gaps.module_gaps = ModuleGaps{8, 36};
        gaps.fill_value = -1; // wraps for unsigned types
        const ROI roi{200, 301, 300, 601};
        auto source = pattern<uint16_t>(301, 101);
        auto result =
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{337, 103});
        // chip gap at column 256 -> gapped columns 56, 57
        CHECK(result(0, 55) == source(0, 55));
        CHECK(result(0, 56) == 65535);
        CHECK(result(0, 57) == 65535);
        CHECK(result(0, 58) == source(0, 56));
        // module gap at row 512 -> gapped rows 212 .. 247
        CHECK(result(211, 0) == source(211, 0));
        CHECK(result(212, 0) == 65535);
        CHECK(result(247, 0) == 65535);
        CHECK(result(248, 0) == source(212, 0));
    }
    SECTION("source shape must match the ROI") {
        auto source = pattern<uint16_t>(2, 4);
        CHECK_THROWS_AS(
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()),
                              ROI{0, 5, 0, 2}, DetectorType::Jungfrau, false),
            std::invalid_argument);
        CHECK_THROWS_AS(
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()),
                              ROI{0, 4, 0, 2}, DetectorType::Moench, false),
            std::invalid_argument);
    }
    SECTION("floating point images") {
        const ROI roi{255, 257, 0, 1};
        NDArray<double, 2> source({1, 2});
        source(0, 0) = 1.5;
        source(0, 1) = 2.5;
        GapPixels gaps;
        gaps.fill_value = 0.25;
        auto result =
            insert_gap_pixels(NDView<const double, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        CHECK(result(0, 0) == 1.5);
        CHECK(result(0, 1) == 0.25);
        CHECK(result(0, 2) == 0.25);
        CHECK(result(0, 3) == 2.5);
    }
}

TEST_CASE("Module boundaries receive no gap unless module gaps are set",
          "[GapPixels]") {
    // rows 510..513 of two stacked modules hold the module boundary at 512
    const ROI roi{0, 2, 510, 514};
    auto source = pattern<uint16_t>(4, 2);
    GapPixels gaps;
    gaps.fill_value = 9;
    SECTION("fill value") {}
    SECTION("split counts") {
        gaps.split_counts = true;
        gaps.seed = 1;
    }
    auto result = insert_gap_pixels(NDView<const uint16_t, 2>(source.view()),
                                    roi, DetectorType::Jungfrau, false, gaps);
    REQUIRE(result.shape() == std::array<ssize_t, 2>{4, 2});
    CHECK(result == source);

    gaps.module_gaps = ModuleGaps{0, 3};
    auto with_gap = insert_gap_pixels(NDView<const uint16_t, 2>(source.view()),
                                      roi, DetectorType::Jungfrau, false, gaps);
    REQUIRE(with_gap.shape() == std::array<ssize_t, 2>{7, 2});
    for (ssize_t row = 2; row < 5; ++row) {
        CHECK(with_gap(row, 0) == 9);
        CHECK(with_gap(row, 1) == 9);
    }
    CHECK(with_gap(1, 0) == source(1, 0));
    CHECK(with_gap(5, 0) == source(2, 0));
}

TEST_CASE("Split counts between edge pixels and their gaps", "[GapPixels]") {
    GapPixels gaps;
    gaps.split_counts = true;
    gaps.seed = 42;

    SECTION("one boundary, integer counts") {
        // columns 254..258 and rows 0..1: one chip boundary at 256
        const ROI roi{254, 258, 0, 2};
        NDArray<uint32_t, 2> source({2, 4});
        source(0, 0) = 10;
        source(0, 1) = 7; // odd, splits 3 + 4 in random order
        source(0, 2) = 9;
        source(0, 3) = 12;
        source(1, 0) = 1;
        source(1, 1) = 8;
        source(1, 2) = 2;
        source(1, 3) = 3;
        auto result =
            insert_gap_pixels(NDView<const uint32_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{2, 6});
        for (ssize_t row = 0; row < 2; ++row) {
            CAPTURE(row);
            CHECK(result(row, 0) == source(row, 0));
            CHECK(result(row, 5) == source(row, 3));
            // left edge pixel and its gap pixel to the right
            CHECK(result(row, 1) + result(row, 2) == source(row, 1));
            CHECK(std::max(result(row, 1), result(row, 2)) ==
                  (source(row, 1) + 1) / 2);
            // right edge pixel and its gap pixel to the left
            CHECK(result(row, 3) + result(row, 4) == source(row, 2));
            CHECK(std::max(result(row, 3), result(row, 4)) ==
                  (source(row, 2) + 1) / 2);
        }
        CHECK(result(0, 1) + result(0, 2) == 7);
        CHECK(result(1, 1) == 4);
        CHECK(result(1, 2) == 4);
        CHECK(result(1, 3) == 1);
        CHECK(result(1, 4) == 1);
    }
    SECTION("corner pixels split four ways") {
        const ROI roi{255, 257, 255, 257};
        NDArray<uint16_t, 2> source({2, 2});
        source(0, 0) = 9;
        source(0, 1) = 8;
        source(1, 0) = 4;
        source(1, 1) = 1;
        auto result =
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{4, 4});
        auto quad_sum = [&](ssize_t row, ssize_t col) {
            return result(row, col) + result(row, col + 1) +
                   result(row + 1, col) + result(row + 1, col + 1);
        };
        CHECK(quad_sum(0, 0) == 9);
        CHECK(quad_sum(0, 2) == 8);
        CHECK(quad_sum(2, 0) == 4);
        CHECK(quad_sum(2, 2) == 1);
        CHECK(result(0, 2) + result(0, 3) == 4);
        CHECK(result(1, 2) + result(1, 3) == 4);
        CHECK(result(2, 0) == 1);
        CHECK(result(2, 1) == 1);
        CHECK(result(3, 0) == 1);
        CHECK(result(3, 1) == 1);
        CHECK(total(NDView<const uint16_t, 2>(result.view())) == 22);
    }
    SECTION("floating point halves exactly") {
        const ROI roi{255, 257, 255, 257};
        NDArray<double, 2> source({2, 2}, 1.0);
        source(0, 0) = 3.0;
        auto result =
            insert_gap_pixels(NDView<const double, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        for (ssize_t row = 0; row < 2; ++row) {
            for (ssize_t col = 0; col < 2; ++col) {
                CHECK(result(row, col) == 0.75);
            }
        }
        CHECK(result(0, 2) == 0.25);
        CHECK(result(3, 3) == 0.25);
    }
    SECTION("totals, remainders and reproducibility on a module") {
        const ROI roi{0, 1024, 0, 512};
        NDArray<uint32_t, 2> source({512, 1024});
        for (ssize_t row = 0; row < 512; ++row) {
            for (ssize_t col = 0; col < 1024; ++col) {
                source(row, col) =
                    static_cast<uint32_t>((row * 7 + col * 13) % 1000 + 1);
            }
        }
        auto first =
            insert_gap_pixels(NDView<const uint32_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        auto second =
            insert_gap_pixels(NDView<const uint32_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        REQUIRE(first.shape() == std::array<ssize_t, 2>{514, 1030});
        CHECK(total(NDView<const uint32_t, 2>(first.view())) ==
              total(NDView<const uint32_t, 2>(source.view())));
        CHECK(first == second);

        // remainders of odd left edge pixels should land on either side
        // about equally often
        size_t odd = 0;
        size_t kept_high = 0;
        for (ssize_t row = 0; row < 512; ++row) {
            if (row == 255 || row == 256) {
                continue; // rows that are split again vertically
            }
            for (ssize_t boundary : {256, 512, 768}) {
                const auto value = source(row, boundary - 1);
                if (value % 2 == 0) {
                    continue;
                }
                ++odd;
                const ssize_t gapped_row = row < 256 ? row : row + 2;
                // gapped column of the left edge pixel, source column boundary
                // - 1
                const ssize_t left = boundary - 1 + 2 * (boundary / 256 - 1);
                if (first(gapped_row, left) > first(gapped_row, left + 1)) {
                    ++kept_high;
                }
            }
        }
        REQUIRE(odd > 500);
        const double fraction = static_cast<double>(kept_high) / odd;
        CHECK(fraction > 0.4);
        CHECK(fraction < 0.6);

        GapPixels other = gaps;
        other.seed = 43;
        auto third =
            insert_gap_pixels(NDView<const uint32_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, other);
        CHECK(third != first);
    }
    SECTION("module gaps keep the fill value") {
        GapPixels module_gaps = gaps;
        module_gaps.module_gaps = ModuleGaps{8, 36};
        module_gaps.fill_value = 3;
        const ROI roi{0, 1024, 0, 1024};
        NDArray<uint16_t, 2> source({1024, 1024}, uint16_t{7});
        auto result =
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, module_gaps);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{1064, 1030});
        for (ssize_t row = 514; row < 550; ++row) {
            for (ssize_t col = 0; col < 1030; ++col) {
                if (result(row, col) != 3) {
                    CAPTURE(row, col);
                    REQUIRE(result(row, col) == 3);
                }
            }
        }
        // the edge pixels next to the module gap are not split
        CHECK(result(513, 0) == 7);
        CHECK(result(550, 0) == 7);
        // chip gaps are split: 7 -> 3 + 4
        CHECK(result(0, 255) + result(0, 256) == 7);
        CHECK(total(NDView<const uint16_t, 2>(result.view())) ==
              7.0 * 1024 * 1024 + 3.0 * 36 * 1030);
    }
    SECTION("no split at a ROI border") {
        const ROI roi{256, 260, 0, 1};
        NDArray<uint16_t, 2> source({1, 4}, uint16_t{9});
        auto result =
            insert_gap_pixels(NDView<const uint16_t, 2>(source.view()), roi,
                              DetectorType::Jungfrau, false, gaps);
        REQUIRE(result.shape() == std::array<ssize_t, 2>{1, 4});
        CHECK(result(0, 0) == 9);
    }
}
