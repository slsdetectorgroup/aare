#include "aare/defs.hpp"
#include "aare/FileFactory.hpp"
#include "aare/NumpyFile.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

template <DetectorType detector, typename DataType> class NumpyFileFactory : public FileFactory<detector, DataType> {
  public:
    NumpyFileFactory(std::filesystem::path fpath);
    void parse_metadata(File<detector, DataType> *_file) override;
    void open_subfiles(File<detector, DataType> *_file) override;
    File<detector, DataType> *load_file() override;

    uint8_t major_ver() const noexcept { return major_ver_; }
    uint8_t minor_ver() const noexcept { return minor_ver_; }

  private:
    static constexpr std::array<char, 6> magic_str{'\x93', 'N', 'U', 'M', 'P', 'Y'};
    uint8_t major_ver_{};
    uint8_t minor_ver_{};
    uint32_t header_len{};
    uint8_t header_len_size{};
    const uint8_t magic_string_length{6};
};