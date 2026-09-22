#pragma once
#include "aare/utils/cuda_check.cuh"
#include <cuda_runtime.h>
#include <memory>

namespace aare {

template <typename T> struct CudaFreeDeleter {
    void operator()(T *p) const noexcept {
        if (p)
            cudaFree(p);
    }
};

template <typename T> class DeviceBuffer {
    std::unique_ptr<T, CudaFreeDeleter<T>> m_ptr{nullptr};
    size_t m_n{0};

  public:
    DeviceBuffer() = default;
    explicit DeviceBuffer(size_t N) : m_n{N} {
        T *raw_ptr = nullptr;
        CUDA_CHECK(cudaMalloc(&raw_ptr, m_n * sizeof(T)));
        m_ptr.reset(raw_ptr);
    }

    DeviceBuffer(DeviceBuffer &&) noexcept = default;
    DeviceBuffer &operator=(DeviceBuffer &&) noexcept = default;

    DeviceBuffer(const DeviceBuffer &) = delete;
    DeviceBuffer &operator=(const DeviceBuffer &) = delete;

    T *get() const noexcept { return m_ptr.get(); }
    size_t size() const noexcept { return m_n; }
    size_t bytes() const noexcept { return m_n * sizeof(T); }
};

struct StreamDeleter {
    void operator()(cudaStream_t s) const noexcept {
        if (s) {
            cudaStreamSynchronize(s);
            cudaStreamDestroy(s);
        }
    }
};

using Stream = std::unique_ptr<CUstream_st, StreamDeleter>;

inline Stream make_stream() {
    cudaStream_t raw_ptr = nullptr;
    CUDA_CHECK(cudaStreamCreateWithFlags(&raw_ptr, cudaStreamNonBlocking));
    return Stream{raw_ptr};
}

} // namespace aare