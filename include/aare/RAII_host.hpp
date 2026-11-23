#pragma once
#include "aare/RAII_device.hpp"
#include "aare/utils/cuda_check.cuh"
#include <cuda_runtime.h>
#include <memory>

namespace aare::cuda {

// Pinned (page-locked) host buffer, freed with cudaFreeHost
struct CudaFreeHostDeleter {
    void operator()(void *p) const noexcept {
        if (p)
            cudaFreeHost(p);
    }
};

using PinnedHostBuffer = std::unique_ptr<void, CudaFreeHostDeleter>;

inline PinnedHostBuffer make_pinned_host(size_t bytes) {
    void *p = nullptr;
    CUDA_CHECK(cudaMallocHost(&p, bytes));
    return PinnedHostBuffer{p};
}

// Overload to create an event with custom flags.
// The default make_event() creates an event without timing,
// whose synchronize sleeps instead of spinning
inline Event make_event(unsigned int flags) {
    cudaEvent_t raw_ptr = nullptr;
    CUDA_CHECK(cudaEventCreateWithFlags(&raw_ptr, flags));
    return Event{raw_ptr};
}

} // namespace aare::cuda