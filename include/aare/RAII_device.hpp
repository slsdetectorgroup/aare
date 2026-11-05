#pragma once
#include "aare/utils/cuda_check.cuh"
#include <cuda_runtime.h>
#include <memory>

namespace aare::cuda {

// Wrapper around cuda device buffers
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

// Host registration wrapper
struct HostRegisterDeleter {
    void operator()(void *p) const noexcept {
        if (p)
            cudaHostUnregister(p);
    }
};

class HostRegistration {
    std::unique_ptr<void, HostRegisterDeleter> m_ptr{nullptr};

  public:
    HostRegistration() = default;
    explicit HostRegistration(void *raw_ptr, size_t bytes) {
        CUDA_CHECK(cudaHostRegister(raw_ptr, bytes, cudaHostRegisterDefault));
        m_ptr.reset(raw_ptr);
    }

    HostRegistration(HostRegistration &&) noexcept = default;
    HostRegistration &operator=(HostRegistration &&) noexcept = default;

    HostRegistration(const HostRegistration &) = delete;
    HostRegistration &operator=(const HostRegistration &) = delete;

    void unregister() {
        if (void *p = m_ptr.release()) {       // m_ptr is now null
            CUDA_CHECK(cudaHostUnregister(p)); // may throw; no retry in dtor
        }
    }
};

// Async allocator wrapper
struct AsyncFreeDeleter {
    cudaStream_t stream;
    void operator()(void *p) const noexcept {
        if (p)
            cudaFreeAsync(p, stream);
    }
};

template <typename T>
using AsyncDeviceBuffer = std::unique_ptr<T, AsyncFreeDeleter>;

template <typename T>
AsyncDeviceBuffer<T> make_async_buffer(size_t n, cudaStream_t stream) {
    T *p = nullptr;
    CUDA_CHECK(cudaMallocAsync(&p, n * sizeof(T), stream));
    return AsyncDeviceBuffer<T>{p, AsyncFreeDeleter{stream}};
}

// cudaStream wrapper
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

// cudaEvent wrapper
struct EventDeleter {
    void operator()(cudaEvent_t e) const noexcept {
        if (e) {
            cudaEventDestroy(e);
        }
    }
};

using Event = std::unique_ptr<CUevent_st, EventDeleter>;

inline Event make_event() {
    cudaEvent_t raw_ptr = nullptr;
    CUDA_CHECK(cudaEventCreateWithFlags(&raw_ptr, cudaEventDisableTiming |
                                                      cudaEventBlockingSync));
    return Event{raw_ptr};
}

} // namespace aare::cuda