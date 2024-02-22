#pragma once
#include "defs.hpp"
#include <cstdint>
#include <filesystem>
#include <variant>

class SubFileBase {
  protected:
    FILE *fp = nullptr;

  public:
    virtual inline size_t bytes_per_frame() =0;
    virtual inline size_t pixels_per_frame() =0;
    std::filesystem::path fname;

    ssize_t rows{};
    ssize_t cols{};
    ssize_t n_frames{};
    int sub_file_index_{};
    virtual size_t read_impl(std::byte *buffer)=0;
    virtual size_t get_frame(std::byte *buffer, int frame_number)=0;

};

template <class Header, class DataType> class SubFile : public SubFileBase{
  public:
    SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    inline size_t bytes_per_frame() override { return sizeof(DataType) * rows * cols; }
    inline size_t pixels_per_frame() override { return rows * cols; }
    virtual size_t read_impl(std::byte *buffer)=0;
    size_t get_frame(std::byte *buffer, int frame_number);

    
};

template <class Header, class DataType> class NormalSubFile : public SubFile<Header, DataType> {
  public:
    NormalSubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    size_t read_impl(std::byte *buffer);
};

template <class Header, class DataType> class ReorderM03SubFile : public SubFile<Header, DataType> {
  public:
    ReorderM03SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols);
    size_t read_impl(std::byte *buffer);
};

using JungfrauSubFile = NormalSubFile<sls_detector_header, uint16_t>;
using Moench03SubFile = ReorderM03SubFile<sls_detector_header, uint16_t>;
using Mythen3SubFile = NormalSubFile<sls_detector_header, uint32_t>;

// using SubFileVariants = std::variant<JungfrauSubFile, Mythen3SubFile, Moench03SubFile>;

