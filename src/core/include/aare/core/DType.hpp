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
class DType {
  public:
    enum TypeIndex { INT8, UINT8, INT16, UINT16, INT32, UINT32, INT64, UINT64, FLOAT, DOUBLE, ERROR };


    uint8_t bitdepth() const;
    uint8_t bytes() const;
    std::string format_descr() const { 
        return std::string(1, DTYPE_FORMAT_DSC[static_cast<int>(m_type)]);
    }


    explicit DType(const std::type_info &t);
    explicit DType(std::string_view sv);

    // not explicit to allow conversions form enum to DType
    DType(DType::TypeIndex ti); // NOLINT

    bool operator==(const DType &other) const noexcept;
    bool operator!=(const DType &other) const noexcept;
    bool operator==(const std::type_info &t) const;
    bool operator!=(const std::type_info &t) const;

    // bool operator==(DType::TypeIndex ti) const;
    // bool operator!=(DType::TypeIndex ti) const;
    std::string to_string() const;
    void set_type(DType::TypeIndex ti) { m_type = ti; }

  private:
    TypeIndex m_type{TypeIndex::ERROR};
};

} // namespace aare