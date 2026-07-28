#include "aare/NDView.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>

namespace aare {

enum STORAGEFORMAT : uint8_t { ROWMAJOR = 0, COLUMNMAJOR = 1 };

/**
 * @brief A class representing a sparse mask for a 2D array.
 *
 * The SparseMask class allows for efficient storage and retrieval of non-zero
 * elements in a 2D array. It supports both row-major and column-major storage
 * formats.
 */
class SparseMask {

  public:
    SparseMask(const STORAGEFORMAT storage_format, const size_t rows,
               const size_t cols);

    SparseMask(NDView<bool, 2> mask, const STORAGEFORMAT storage_format);

    // TODO: think of storage hdf5 probably the best innerindices, outerindices
    // as datasets and storage_format as attribute
    SparseMask(const std::filesystem::path &filename);

    void insert(const size_t row, const size_t col);

    /**
     * @brief Check if the element at (row, col) is masked (non-zero).
     * @param row Row index of the element.
     * @param col Column index of the element.
     * @return true if the element is masked (non-zero), false otherwise.
     */
    bool is_masked(const size_t row, const size_t col) const;

    void write_to_file(const std::filesystem::path &filename) const;

    /// @brief Get number of bad channels
    size_t num_bad_channels() const;

  private:
    /// @brief stoarge format of the sparse mask, either row major or column
    /// major
    STORAGEFORMAT storage_format_;

    /// @brief for column major stores row indices of non-zero elements, for row
    /// major stores column indices of non-zero elements
    std::vector<uint32_t> innerindices_;

    /// @brief for column major outerindices[j] gives the index in innerindices_
    /// of the first non-zero element in column j, for row major outerindices[i]
    /// gives the index in innerindices_ of the first non-zero element in row i
    std::vector<uint32_t> outerindices_;
};

} // namespace aare