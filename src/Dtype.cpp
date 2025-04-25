
#include "aare/Dtype.hpp"
#include "aare/defs.hpp"

#include <fmt/core.h>

namespace aare {

/**
 * @brief Construct a DType object from a type_info object
 * @param t type_info object
 * @throw runtime_error if the type is not supported
 * @note supported types are: int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double
 * @note the type_info object is obtained using typeid (e.g. typeid(int))
 */
Dtype::Dtype(const std::type_info &t) {
    if (t == typeid(int8_t))
        m_type = TypeIndex::INT8;
    else if (t == typeid(uint8_t))
        m_type = TypeIndex::UINT8;
    else if (t == typeid(int16_t))
        m_type = TypeIndex::INT16;
    else if (t == typeid(uint16_t))
        m_type = TypeIndex::UINT16;
    else if (t == typeid(int32_t))
        m_type = TypeIndex::INT32;
    else if (t == typeid(uint32_t))
        m_type = TypeIndex::UINT32;
    else if (t == typeid(int64_t)) // NOLINT
        m_type = TypeIndex::INT64;
    else if (t == typeid(uint64_t))
        m_type = TypeIndex::UINT64;
    else if (t == typeid(float))
        m_type = TypeIndex::FLOAT;
    else if (t == typeid(double))
        m_type = TypeIndex::DOUBLE;
    else
        throw std::runtime_error("Could not construct data type. Type not supported.");
}

/**
 * @brief Get the bitdepth of the data type
 * @return bitdepth
 */
uint8_t Dtype::bitdepth() const {
    switch (m_type) {
    case TypeIndex::INT8:
    case TypeIndex::UINT8:
        return 8;
    case TypeIndex::INT16:
    case TypeIndex::UINT16:
        return 16;
    case TypeIndex::INT32:
    case TypeIndex::UINT32:
        return 32;
    case TypeIndex::INT64:
    case TypeIndex::UINT64:
        return 64;
    case TypeIndex::FLOAT:
        return 32;
    case TypeIndex::DOUBLE:
        return 64;
    case TypeIndex::NONE:
        return 0;
    default:
        throw std::runtime_error(LOCATION + "Could not get bitdepth. Type not supported.");
    }
}

/**
 * @brief Get the number of bytes of the data type
 */
size_t Dtype::bytes() const { return bitdepth() / bits_per_byte; }

/**
 * @brief Construct a DType object from a TypeIndex
 * @param ti TypeIndex
 *
 */
Dtype::Dtype(Dtype::TypeIndex ti) : m_type(ti) {}

/**
 * @brief Construct a DType object from a string
 * @param sv string_view
 * @throw runtime_error if the type is not supported
 * @note example strings: "<i4", "u8", "f4"
 * @note the endianess is checked and only native endianess is supported
 */
Dtype::Dtype(std::string_view sv) {

    // Check if the file is using our native endianess
    if (auto pos = sv.find_first_of("<>"); pos != std::string_view::npos) {
        const auto endianess = [](const char c) {
            if (c == '<')
                return endian::little;
            return endian::big;
        }(sv[pos]);
        if (endianess != endian::native) {
            throw std::runtime_error("Non native endianess not supported");
        }
    }

    // we are done with the endianess so we can remove the prefix
    sv.remove_prefix(std::min(sv.find_first_not_of("<>"), sv.size()));

    if (sv == "i1")
        m_type = TypeIndex::INT8;
    else if (sv == "u1")
        m_type = TypeIndex::UINT8;
    else if (sv == "i2")
        m_type = TypeIndex::INT16;
    else if (sv == "u2")
        m_type = TypeIndex::UINT16;
    else if (sv == "i4")
        m_type = TypeIndex::INT32;
    else if (sv == "u4")
        m_type = TypeIndex::UINT32;
    else if (sv == "i8")
        m_type = TypeIndex::INT64;
    else if (sv == "u8")
        m_type = TypeIndex::UINT64;
    else if (sv == "f4")
        m_type = TypeIndex::FLOAT;
    else if (sv == "f8")
        m_type = TypeIndex::DOUBLE;
    else
        throw std::runtime_error("Cannot construct data type from string.");
}

Dtype Dtype::from_bitdepth(uint8_t bitdepth) {
    switch (bitdepth) {
    case 8:
        return Dtype(TypeIndex::UINT8);
    case 16:
        return Dtype(TypeIndex::UINT16);
    case 32:
        return Dtype(TypeIndex::UINT32);
    case 64:
        return Dtype(TypeIndex::UINT64);
    default:
        throw std::runtime_error("Could not construct data type from bitdepth.");
    }
}
/**
 * @brief Get the string representation of the data type
 * @return string representation
 */
std::string Dtype::to_string() const {

    char ec{};
    if (endian::native == endian::little)
        ec = '<';
    else
        ec = '>';

    switch (m_type) {
    case TypeIndex::INT8:
        return fmt::format("{}i1", ec);
    case TypeIndex::UINT8:
        return fmt::format("{}u1", ec);
    case TypeIndex::INT16:
        return fmt::format("{}i2", ec);
    case TypeIndex::UINT16:
        return fmt::format("{}u2", ec);
    case TypeIndex::INT32:
        return fmt::format("{}i4", ec);
    case TypeIndex::UINT32:
        return fmt::format("{}u4", ec);
    case TypeIndex::INT64:
        return fmt::format("{}i8", ec);
    case TypeIndex::UINT64:
        return fmt::format("{}u8", ec);
    case TypeIndex::FLOAT:
        return "f4";
    case TypeIndex::DOUBLE:
        return "f8";
    case TypeIndex::ERROR:
        throw std::runtime_error("Could not get string representation. Type not supported.");
    case TypeIndex::NONE:
        throw std::runtime_error("Could not get string representation. Type not supported.");
    }
    return {};
}

bool Dtype::operator==(const Dtype &other) const noexcept { return m_type == other.m_type; }
bool Dtype::operator!=(const Dtype &other) const noexcept { return !(*this == other); }

bool Dtype::operator==(const std::type_info &t) const { return Dtype(t) == *this; }
bool Dtype::operator!=(const std::type_info &t) const { return Dtype(t) != *this; }

} // namespace aare
