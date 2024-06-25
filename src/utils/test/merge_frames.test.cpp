#include "aare/utils/merge_frames.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;
// void merge_frames(std::vector<std::byte *> &part_buffers, size_t part_size, std::byte *merged_frame, const xy
// &geometry,
//                   size_t cols = 0, size_t rows = 0, size_t bitdepth = 0) {
TEST_CASE("merge frames {2,1}") {
    xy geo = {2, 1};
    std::vector<uint32_t> p1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint32_t> p2 = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    size_t part_size = p1.size() * sizeof(uint32_t);
    Frame f(10, 2, Dtype::UINT32);
    std::vector<std::byte *> part_buffers = {reinterpret_cast<std::byte *>(p1.data()),
                                             reinterpret_cast<std::byte *>(p2.data())};
    merge_frames(part_buffers, part_size, f.data(), geo);

    auto v = f.view<uint32_t>();
    for (int64_t i = 0; i < v.size(); i++) {
        REQUIRE(v[i] == i);
    }
}
TEST_CASE("merge frames {1,2}") {
    xy geo = {1, 2};
    std::vector<uint32_t> p1 = {0, 1, 2, 3, 4, 10, 11, 12, 13, 14};
    std::vector<uint32_t> p2 = {5, 6, 7, 8, 9, 15, 16, 17, 18, 19};
    size_t part_size = p1.size() * sizeof(uint32_t);
    Frame f(2, 10, Dtype::UINT32);
    std::vector<std::byte *> part_buffers = {reinterpret_cast<std::byte *>(p1.data()),
                                             reinterpret_cast<std::byte *>(p2.data())};
    merge_frames(part_buffers, part_size, f.data(), geo, 2, 10, 32);

    auto v = f.view<uint32_t>();

    for (int64_t i = 0; i < v.size(); i++) {
        REQUIRE(v[i] == i);
    }
}
