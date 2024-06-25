#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <typeinfo>

namespace aare {

// https://github.com/pybind/pybind11/issues/1908#issuecomment-658358767
// in the following order:
// int8, uint8, int16, uint16, int32, uint32, int64, uint64, float, double
// if it's windows
#if defined(_WIN32) || defined(_WIN64)
const char DTYPE_FORMAT_DSC[] = {'b', 'B', 'h', 'H', 'l', 'L', 'q', 'Q', 'f', 'd'};

// if it's apple of linux64
#elif defined(__APPLE__) || (defined(__linux__) && (defined(__x86_64__) || defined(__ppc64__)))
const char DTYPE_FORMAT_DSC[] = {'b', 'B', 'h', 'H', 'i', 'I', 'l', 'L', 'f', 'd'};

// if it's linux32
#elif defined(__linux__) && defined(__i386__)
const char DTYPE_FORMAT_DSC[] = {'b', 'B', 'h', 'H', 'l', 'L', 'q', 'Q', 'f', 'd'};
#endif
/**
 * @brief enum class to define the endianess of the system
 */
enum class endian {
#ifdef _WIN32
    little = 0,
    big = 1,
    native = little
#else
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#endif
};

/**
 * @brief class to define the data type of the pixels
 * @note only native endianess is supported
 */
class Dtype {
  public:
    enum TypeIndex { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, ERROR, NONE };

    uint8_t bitdepth() const;
    uint8_t bytes() const;
    std::string format_descr() const { return std::string(1, DTYPE_FORMAT_DSC[static_cast<int>(m_type)]); }

    explicit Dtype(const std::type_info &t);
    explicit Dtype(std::string_view sv);
    static Dtype from_bitdepth(uint8_t bitdepth);    

    // not explicit to allow conversions form enum to DType
    Dtype(Dtype::TypeIndex ti); // NOLINT

    bool operator==(const Dtype &other) const noexcept;
    bool operator!=(const Dtype &other) const noexcept;
    bool operator==(const std::type_info &t) const;
    bool operator!=(const std::type_info &t) const;

    // bool operator==(DType::TypeIndex ti) const;
    // bool operator!=(DType::TypeIndex ti) const;
    std::string to_string() const;
    void set_type(Dtype::TypeIndex ti) { m_type = ti; }

  private:
    TypeIndex m_type{TypeIndex::ERROR};
};

} // namespace aare