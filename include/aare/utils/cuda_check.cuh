#pragma once
#include <cuda_runtime.h>
#include <sstream>
#include <stdexcept>

inline void __cuda_check(cudaError_t err, const char *file, int line) {
    if (err != cudaSuccess) {
        throw std::runtime_error((std::ostringstream{}
                                  << "[CUDA ERROR] " << cudaGetErrorString(err)
                                  << " at " << file << ":" << line)
                                     .str());
    }
}

#define CUDA_CHECK(stmt) __cuda_check((stmt), __FILE__, __LINE__)