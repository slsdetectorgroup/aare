#include "aare/utils/SparseMask.hpp"

namespace aare {

SparseMask::SparseMask(const STORAGEFORMAT storage_format, const size_t rows,
                       const size_t cols)
    : storage_format_(storage_format), rows_(rows), cols_(cols) {
    if (storage_format_ == STORAGEFORMAT::ROWMAJOR) {
        outerindices_.resize(rows_ + 1, 0);
    } else if (storage_format_ == STORAGEFORMAT::COLUMNMAJOR) {
        outerindices_.resize(cols_ + 1, 0);
    } else {
        throw std::invalid_argument(
            "Invalid storage format: must be either ROWMAJOR or COLUMNMAJOR");
    }

    innerindices_.reserve(rows_ * cols_); // Reserve maximum possible size
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
        // TODO: binary search does not work if not filled along cols e.g. rows
        // e.g. random row col insert
        return std::binary_search(innerindices_.begin() + start,
                                  innerindices_.begin() + end, nonzero_index);
    }
}

size_t SparseMask::num_bad_channels() const { return innerindices_.size(); }

NDArray<bool, 2> SparseMask::convert_to_dense() const {
    NDArray<bool, 2> dense_mask{
        std::array<ssize_t, 2>{static_cast<ssize_t>(rows_),
                               static_cast<ssize_t>(cols_)},
        false};

    for (size_t i = 0; i < outerindices_.size() - 1; ++i) {
        size_t start = outerindices_[i];
        size_t end = outerindices_[i + 1];
        for (size_t j = start; j < end; ++j) {
            if (storage_format_ == STORAGEFORMAT::ROWMAJOR) {
                dense_mask(i, innerindices_[j]) = true;
            } else {
                dense_mask(innerindices_[j], i) = true;
            }
        }
    }
    return dense_mask;
}

} // namespace aare