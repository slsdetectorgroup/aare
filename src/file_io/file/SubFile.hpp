#pragma once
#include "defs.hpp"
#include <cstdint>
#include <filesystem>
#include <variant>

template <class Header, class DataType> class SubFile {
  protected:
    FILE *fp = nullptr;

  public:
    SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    inline size_t bytes_per_frame() const { return sizeof(DataType) * rows * cols; }

    inline size_t pixels_per_frame() const { return rows * cols; }


    ssize_t rows{};
    ssize_t cols{};
    ssize_t n_frames{};
    int sub_file_index_{};
    size_t read_impl(std::byte *buffer){};
    
};

template <class Header, class DataType> class NormalSubFile : public SubFile<Header, DataType> {
  public:
    NormalSubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    size_t read_impl(std::byte *buffer);
};

// template <class Header, class DataType>
// class FlipSubFile : public SubFile<Header, DataType>{
//     size_t read_impl() override;
// };

template <class Header, class DataType> class ReorderM03SubFile : public SubFile<Header, DataType> {
  public:
    ReorderM03SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    size_t read_impl(std::byte *buffer);
};

using JungfrauSubFile = NormalSubFile<sls_detector_header, uint16_t>;
using Moench03SubFile = ReorderM03SubFile<sls_detector_header, uint16_t>;
using Mythen3SubFile = NormalSubFile<sls_detector_header, uint32_t>;

using SubFileVariants = std::variant<JungfrauSubFile, Mythen3SubFile, Moench03SubFile>;
