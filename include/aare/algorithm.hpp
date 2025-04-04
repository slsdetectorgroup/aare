
#pragma once
#include <algorithm>
#include <array>
#include <vector>
#include <aare/NDArray.hpp>

namespace aare {
/**
 * @brief Find the index of the last element smaller than val
 * assume a sorted array
 */
template <typename T>
size_t last_smaller(const T* first, const T* last, T val) {
    for (auto iter = first+1; iter != last; ++iter) {
        if (*iter > val) {
            return std::distance(first, iter-1);
        }
    }
    return std::distance(first, last-1);
}

template <typename T>
size_t last_smaller(const NDArray<T, 1>& arr, T val) {
    return last_smaller(arr.begin(), arr.end(), val);
}

template <typename T>
size_t first_larger(const T* first, const T* last, T val) {
    for (auto iter = first; iter != last; ++iter) {
        if (*iter > val) {
            return std::distance(first, iter);
        }
    }
    return std::distance(first, last-1);
}

template <typename T>
size_t first_larger(const NDArray<T, 1>& arr, T val) {
    return first_larger(arr.begin(), arr.end(), val);
}

template <typename T>
size_t first_larger(const std::vector<T>& vec, T val) {
    return first_larger(vec.data(), vec.data()+vec.size(), val);
}

template <typename T>
size_t nearest_index(const T* first, const T* last, T val) {
    auto iter = std::min_element(first, last,
    [val](T a, T b) {
        return std::abs(a - val) < std::abs(b - val);
    });
    return std::distance(first, iter);
}

template <typename T>
size_t nearest_index(const NDArray<T, 1>& arr, T val) {
    return nearest_index(arr.begin(), arr.end(), val);
}

template <typename T>
size_t nearest_index(const std::vector<T>& vec, T val) {
    return nearest_index(vec.data(), vec.data()+vec.size(), val);
}

template <typename T, size_t N>
size_t nearest_index(const std::array<T,N>& arr, T val) {
    return nearest_index(arr.data(), arr.data()+arr.size(), val);
}

template <typename T>
std::vector<T> cumsum(const std::vector<T>& vec) {
    std::vector<T> result(vec.size());
    std::partial_sum(vec.begin(), vec.end(), result.begin());
    return result;
}



} // namespace aare