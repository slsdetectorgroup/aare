// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <aare/NDArray.hpp>
#include <algorithm>
#include <array>
#include <vector>

namespace aare {
/**
 * @brief Index of the last element that is smaller than val.
 * Requires a sorted array. Uses >= for ordering. If all elements
 * are smaller it returns the last element and if all elements are
 * larger it returns the first element.
 * @param first iterator to the first element
 * @param last iterator to the last element
 * @param val value to compare
 * @return index of the last element that is smaller than val
 *
 */
template <typename T>
size_t last_smaller(const T *first, const T *last, T val) {
    for (auto iter = first + 1; iter != last; ++iter) {
        if (*iter >= val) {
            return std::distance(first, iter - 1);
        }
    }
    return std::distance(first, last - 1);
}

template <typename T> size_t last_smaller(const NDArray<T, 1> &arr, T val) {
    return last_smaller(arr.begin(), arr.end(), val);
}

template <typename T> size_t last_smaller(const std::vector<T> &vec, T val) {
    return last_smaller(vec.data(), vec.data() + vec.size(), val);
}

/**
 * @brief Index of the first element that is larger than val.
 * Requires a sorted array. Uses > for ordering. If all elements
 * are larger it returns the first element and if all elements are
 * smaller it returns the last element.
 * @param first iterator to the first element
 * @param last iterator to the last element
 * @param val value to compare
 * @return index of the first element that is larger than val
 */
template <typename T>
size_t first_larger(const T *first, const T *last, T val) {
    for (auto iter = first; iter != last; ++iter) {
        if (*iter > val) {
            return std::distance(first, iter);
        }
    }
    return std::distance(first, last - 1);
}

template <typename T> size_t first_larger(const NDArray<T, 1> &arr, T val) {
    return first_larger(arr.begin(), arr.end(), val);
}

template <typename T> size_t first_larger(const std::vector<T> &vec, T val) {
    return first_larger(vec.data(), vec.data() + vec.size(), val);
}

/**
 * @brief Index of the nearest element to val.
 * Requires a sorted array. If there is no difference it takes the first
 * element.
 * @param first iterator to the first element
 * @param last iterator to the last element
 * @param val value to compare
 * @return index of the nearest element
 */
template <typename T>
size_t nearest_index(const T *first, const T *last, T val) {
    auto iter = std::min_element(first, last, [val](T a, T b) {
        return std::abs(a - val) < std::abs(b - val);
    });
    return std::distance(first, iter);
}

template <typename T> size_t nearest_index(const NDArray<T, 1> &arr, T val) {
    return nearest_index(arr.begin(), arr.end(), val);
}

template <typename T> size_t nearest_index(const std::vector<T> &vec, T val) {
    return nearest_index(vec.data(), vec.data() + vec.size(), val);
}

template <typename T, size_t N>
size_t nearest_index(const std::array<T, N> &arr, T val) {
    return nearest_index(arr.data(), arr.data() + arr.size(), val);
}

template <typename T> std::vector<T> cumsum(const std::vector<T> &vec) {
    std::vector<T> result(vec.size());
    std::partial_sum(vec.begin(), vec.end(), result.begin());
    return result;
}

template <typename Container> bool all_equal(const Container &c) {
    if (!c.empty() &&
        std::all_of(begin(c), end(c),
                    [c](const typename Container::value_type &element) {
                        return element == c.front();
                    }))
        return true;
    return false;
}

/**
 * linear interpolation
 * @param bin_edge left and right bin edges
 * @param bin_values function values at bin edges
 * @param coord coordinate to interpolate at
 * @return interpolated value at coord
 */
inline double linear_interpolation(const std::pair<double, double> &bin_edge,
                                   const std::pair<double, double> &bin_values,
                                   const double coord) {
    const double bin_width = bin_edge.second - bin_edge.first;
    return bin_values.first * (1 - (coord - bin_edge.first) / bin_width) +
           bin_values.second * (coord - bin_edge.first) / bin_width;
}

/// @brief XOR operator
inline bool XOR(const bool a, const bool b) { return (a || b) && !(a && b); }

/// @brief range struct
template <typename Iterator> struct range {
    /// @brief start of the range
    Iterator start{};
    /// @brief end of the range
    Iterator end{};
};

/**
 * @brief partition range of elements into contiguous subranges for which the
 * partition criteria is the same.
 * @param start pointer to the first element of the range
 * @param end pointer to the last element of the range
 * @param partition_criteria function that returns true or false for a given
 * element
 * @return vector of ranges that are contiguous and have the same partition
 * criteria
 */
template <typename Iterator>
std::vector<range<Iterator>> partition(
    Iterator start, Iterator end,
    std::function<bool(const typename std::iterator_traits<Iterator>::value_type
                           &)> &partition_criteria) {
    std::vector<range<Iterator>> partitions;
    partitions.reserve(std::distance(start, end));

    auto partition_start = start;
    auto partition_end = end;

    bool chunk_fulfills_criteria = partition_criteria(
        *partition_start); // starts with range fulfilling criteria

    auto chunk_criteria =
        [&partition_criteria, &chunk_fulfills_criteria](
            const typename std::iterator_traits<Iterator>::value_type &value) {
            return XOR(partition_criteria(value), chunk_fulfills_criteria);
        }; // exclusive or

    while (partition_start < end) {
        partition_end = std::find_if(partition_start, end, chunk_criteria);

        partitions.push_back({partition_start, partition_end});
        partition_start = partition_end;

        chunk_fulfills_criteria =
            !chunk_fulfills_criteria; // flip criteria for next chunk
    }

    return partitions;
}

} // namespace aare