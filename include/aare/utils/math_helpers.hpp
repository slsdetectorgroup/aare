#pragma once
#include <cstddef>

namespace aare {
    
/**
 * @brief Compute the ceiling of the integer division of n by d.
 * @param n The numerator.
 * @param d The denominator.
 * @return The ceiling of the integer division.
 */
constexpr size_t ceil_div(size_t n, size_t d){
    return n / d + (n % d != 0);
}

} // namespace aare