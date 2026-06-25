#include "aare/utils/SparseMask.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Create Sparse Mask") {

    std::vector<std::pair<size_t, size_t>> masked_pairs = {
        {1, 1}, {1, 2}, {1, 4}, {2, 0}, {2, 2}, {2, 4}, {3, 4}, {4, 1}};

    SECTION("Row Major") {
        aare::SparseMask mask(aare::STORAGEFORMAT::ROWMAJOR, 5, 5);

        std::for_each(masked_pairs.begin(), masked_pairs.end(),
                      [&mask](const std::pair<size_t, size_t> &p) {
                          mask.insert(p.first, p.second);
                      });

        for (size_t row = 0; row < 5; ++row) {
            for (size_t col = 0; col < 5; ++col) {
                if (std::find(masked_pairs.begin(), masked_pairs.end(),
                              std::make_pair(row, col)) != masked_pairs.end()) {
                    CHECK(mask.is_masked(row, col) == true);
                } else {
                    CHECK(mask.is_masked(row, col) == false);
                }
            }
        }
    }
    SECTION("Column Major") {
        aare::SparseMask mask(aare::STORAGEFORMAT::COLUMNMAJOR, 5, 5);

        std::for_each(masked_pairs.begin(), masked_pairs.end(),
                      [&mask](const std::pair<size_t, size_t> &p) {
                          mask.insert(p.first, p.second);
                      });

        for (size_t row = 0; row < 5; ++row) {
            for (size_t col = 0; col < 5; ++col) {
                if (std::find(masked_pairs.begin(), masked_pairs.end(),
                              std::make_pair(row, col)) != masked_pairs.end()) {
                    CHECK(mask.is_masked(row, col) == true);
                } else {
                    CHECK(mask.is_masked(row, col) == false);
                }
            }
        }
    }
}
