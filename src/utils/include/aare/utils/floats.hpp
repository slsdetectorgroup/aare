#include <cfloat>
#include <cmath>
#include <iostream>
namespace aare {
template <typename T = float> bool compare_floats(T A, T B, T maxRelDiff = -1) {
    bool user_defined_diff = true;
    if (maxRelDiff < 0) {
        user_defined_diff = false;
        maxRelDiff = std::numeric_limits<T>::epsilon();
    }
    // Calculate the difference.
    T diff = fabs(A - B);
    A = fabs(A);
    B = fabs(B);
    // Find the largest
    T largest = (B > A) ? B : A;

    // if user defined maxRelDiff then compare to it without scaling
    if (user_defined_diff) {
        if (diff <= maxRelDiff) {
            return true;
        }
        return false;
    } else {
        if (diff <= largest * maxRelDiff)
            return true;
        return false;
    }
}

} // namespace aare