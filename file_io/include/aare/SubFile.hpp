#pragma once
#include "aare/defs.hpp"
#include <cstdint>
#include <filesystem>
#include <variant>
#include <map>

class SubFile {
  protected:
    FILE *fp = nullptr;
    ssize_t m_bitdepth;
    std::filesystem::path m_fname;
    ssize_t m_rows{};
    ssize_t m_cols{};
    ssize_t n_frames{};
    int m_sub_file_index_{};
    // pointer to functions that will read frames
    using pfunc = size_t (SubFile::*)(std::byte *);
    std::map<std::pair<DetectorType, int>, pfunc> read_impl_map = {
        {{DetectorType::Moench, 16}, &SubFile::read_impl_reorder<uint16_t>},
        {{DetectorType::Jungfrau, 16}, &SubFile::read_impl_normal},
        {{DetectorType::ChipTestBoard,16}, &SubFile::read_impl_normal},
        {{DetectorType::Mythen3, 32}, &SubFile::read_impl_normal}
    };


  public:
    // pointer to a read_impl function. pointer will be set to the appropriate read_impl function in the constructor
    pfunc read_impl = nullptr;
    size_t read_impl_normal(std::byte *buffer);
    template <typename DataType> size_t  read_impl_flip(std::byte *buffer);
    template <typename DataType> size_t  read_impl_reorder(std::byte *buffer);


    SubFile(std::filesystem::path fname,DetectorType detector, ssize_t rows, ssize_t cols, uint16_t bitdepth);

    size_t get_part(std::byte *buffer, int frame_number);
    size_t frame_number(int frame_index);

    // TODO: define the inlines as variables and assign them in constructor
    inline size_t bytes_per_part() { return (m_bitdepth / 8) * m_rows * m_cols; }
    inline size_t pixels_per_part() { return m_rows * m_cols; }

};
