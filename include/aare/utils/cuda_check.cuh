#pragma once
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

inline void __cuda_check(cudaError_t err, const char* file, int line) {
  if (err != cudaSuccess) {
    std::fprintf(stderr, "[%s:%d] CUDA error: %s\n",
                 file, line, cudaGetErrorString(err));
    std::exit(1);
  }
}

#define CUDA_CHECK(stmt) __cuda_check((stmt), __FILE__, __LINE__)