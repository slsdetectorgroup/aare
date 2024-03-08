#pragma once
#include "aare/defs.hpp"
#include <cstdint>
#include <filesystem>
#include <variant>

class SubFile {
  protected:
    FILE *fp = nullptr;
    uint16_t bitdepth;

  public:
    // pointer to a read_impl function. pointer will be set to the appropriate read_impl function in the constructor
    size_t (SubFile::*read_impl)(std::byte *buffer) = nullptr;
    size_t read_impl_normal(std::byte *buffer);
    template <typename DataType> size_t  read_impl_flip(std::byte *buffer);
    template <typename DataType> size_t  read_impl_reorder(std::byte *buffer);


    SubFile(std::filesystem::path fname,DetectorType detector, ssize_t rows, ssize_t cols, uint16_t bitdepth);

    size_t get_frame(std::byte *buffer, int frame_number);

    // TODO: define the inlines as variables and assign them in constructor
    inline size_t bytes_per_frame() { return (bitdepth / 8) * rows * cols; }
    inline size_t pixels_per_frame() { return rows * cols; }
    std::filesystem::path fname;
    ssize_t rows{};
    ssize_t cols{};
    ssize_t n_frames{};
    int sub_file_index_{};
};
