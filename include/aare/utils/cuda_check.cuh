#pragma once
#include <cuda_runtime.h>
#include <sstream>
#include <stdexcept>

namespace aare::cuda::detail {
inline void cuda_check(cudaError_t err, const char *file, int line) {
    if (err != cudaSuccess) {
        throw std::runtime_error((std::ostringstream{}
                                  << "[CUDA ERROR] " << cudaGetErrorString(err)
                                  << " at " << file << ":" << line)
                                     .str());
    }
}
} // namespace aare::cuda::detail
#define CUDA_CHECK(stmt)                                                       \
    ::aare::cuda::detail::cuda_check((stmt), __FILE__, __LINE__)