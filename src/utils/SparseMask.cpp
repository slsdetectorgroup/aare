#include "aare/utils/SparseMask.hpp"

namespace aare {

SparseMask::SparseMask(const STORAGEFORMAT storage_format, const size_t rows,
                       const size_t cols)
    : storage_format_(storage_format) {
    if (storage_format_ == STORAGEFORMAT::ROWMAJOR) {
        outerindices_.resize(rows + 1, 0);
    } else if (storage_format_ == STORAGEFORMAT::COLUMNMAJOR) {
        outerindices_.resize(cols + 1, 0);
    } else {
        throw std::invalid_argument(
            "Invalid storage format: must be either ROWMAJOR or COLUMNMAJOR");
    }

    innerindices_.reserve(rows * cols); // Reserve maximum possible size
}

void SparseMask::insert(const size_t row, const size_t col) {

    // TODO: can be very inefficient -> most generic so does not depend on row,
    // col order of insertion
    // -> maybe better std::vector<std::vector>> for each row, col flatten after
    // inserting all -> create innerindices from size

    if (storage_format_ == STORAGEFORMAT::ROWMAJOR) {
        std::for_each(outerindices_.begin() + row + 1, outerindices_.end(),
                      [](uint32_t &x) { ++x; });
        innerindices_.insert(innerindices_.begin() + outerindices_[row + 1] - 1,
                             static_cast<uint32_t>(col));
    } else {
        std::for_each(outerindices_.begin() + col + 1, outerindices_.end(),
                      [](uint32_t &x) { ++x; });
        innerindices_.insert(innerindices_.begin() + outerindices_[col + 1] - 1,
                             static_cast<uint32_t>(row));
    }
}

bool SparseMask::is_masked(const size_t row, const size_t col) const {

    const size_t index_outer_indices =
        storage_format_ == STORAGEFORMAT::ROWMAJOR ? row : col;

    const size_t nonzero_index =
        storage_format_ == STORAGEFORMAT::ROWMAJOR ? col : row;

    if (outerindices_[index_outer_indices + 1] -
            outerindices_[index_outer_indices] ==
        0) {
        return false; // No non-zero elements in this row
    } else {
        auto start = outerindices_[index_outer_indices];
        auto end = outerindices_[index_outer_indices + 1];
<<<<<<< HEAD
        // TODO: binary search does not work if not filled along cols e.g. rows
        // e.g. random row col insert
        return std::binary_search(innerindices_.begin() + start,
                                  innerindices_.begin() + end, nonzero_index);
=======
        for (size_t i = start; i < end; ++i) {
            if (innerindices_[i] == nonzero_index) {
                return true; // Found a non-zero element at (row, col)
            }
        }
        return false; // No non-zero element found at (row, col)
>>>>>>> parent of 3b41a79 (some benchmarks for this useless sparsemask)
    }
}

} // namespace aare