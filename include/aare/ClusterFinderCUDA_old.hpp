// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinder.hpp"
#include "aare/clusterfinder_kernel.cuh"
#include "aare/utils/cuda_check.cuh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace aare {

// Per-stream device resources
template <typename ClusterType, typename FRAME_TYPE, typename PEDESTAL_TYPE>
struct StreamContext {
    cudaStream_t stream = nullptr; // handle to the stream
    FRAME_TYPE *d_frame = nullptr;
    PEDESTAL_TYPE *d_pd_mean = nullptr;
    PEDESTAL_TYPE *d_pd_sum = nullptr;
    PEDESTAL_TYPE *d_pd_sum2 = nullptr;
    ClusterType *d_clusters = nullptr;
    uint32_t *d_cluster_count = nullptr;

    // Pinned host staging buffers.  These make cudaMemcpyAsync real async DMA
    // transfers even when the caller's NDView points to pageable memory.
    FRAME_TYPE *h_frame = nullptr;
    uint32_t *h_cluster_count = nullptr;
    ClusterType *h_clusters = nullptr;

    cudaEvent_t kernel_start = nullptr;
    cudaEvent_t kernel_stop = nullptr;
};

template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinderCUDA {
    using COMPUTE_TYPE =
        device::COMPUTE_TYPE; // match the kernel's internal precision

    static constexpr int BLOCK_X = 16;
    static constexpr int BLOCK_Y = 16;
    static constexpr int col_radius = ClusterType::cluster_size_x / 2;
    static constexpr int row_radius = ClusterType::cluster_size_y / 2;

    Shape<2> m_shape;
    size_t nrows;
    size_t ncols;
    size_t m_image_size; // nrows * ncols
    int n_streams;
    size_t m_capacity;

    size_t m_image_bytes;
    size_t m_cluster_bytes;

    COMPUTE_TYPE m_nSigma;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;
    bool m_pedestal_dirty = true;

    using SC = StreamContext<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    std::vector<SC> v_sc;

    float m_total_kernel_ms = 0.0f;
    size_t m_frames_processed = 0;

    // Kernel parameters
    dim3 grid;
    dim3 block;
    size_t shmem_bytes;

  public:
    /**
     * @brief Construct a ClusterFinderCUDA
     *
     * @param m_image_size              shape of the detector frame (rows, cols)
     * @param nSigma                  threshold in units of per-pixel pedestal
     * std
     * @param capacity                device-side cluster buffer size per stream
     * @param n_streams               number of CUDA streams for multi-frame
     * overlap
     */
    ClusterFinderCUDA(Shape<2> shape_, COMPUTE_TYPE nSigma = 5.0,
                      size_t capacity = 1000000, int n_streams_ = 1)
        : m_shape(shape_), nrows(shape_[0]), ncols(shape_[1]),
          m_image_size(nrows * ncols), n_streams(n_streams_),
          m_capacity(capacity), m_nSigma(nSigma),
          m_pedestal(shape_[0], shape_[1]), m_clusters(capacity) {
        if (n_streams_ <= 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: n_streams must be > 0");
        }

        if (capacity >
            static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: capacity must fit in uint32_t");
        }

        if (capacity == 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: capacity must be > 0");
        }

        // Grid/Block dimensions
        block = dim3(BLOCK_X, BLOCK_Y);
        grid = dim3((static_cast<unsigned int>(ncols) + BLOCK_X - 1) / BLOCK_X,
                    (static_cast<unsigned int>(nrows) + BLOCK_Y - 1) / BLOCK_Y);

        // Shared memory: one tile of (BLOCK_X + 2*col_radius) x (BLOCK_Y +
        // 2*row_radius) elements
        // Mixed precision used -> shmem takes COMPUTE_TYPE = floats (not
        // PEDESTAL_TYPE)
        shmem_bytes = (BLOCK_X + 2 * col_radius) * (BLOCK_Y + 2 * row_radius) *
                      sizeof(COMPUTE_TYPE);

        m_image_bytes = m_image_size * sizeof(FRAME_TYPE);
        m_cluster_bytes = m_capacity * sizeof(ClusterType);

        v_sc.resize(n_streams);
        for (int k = 0; k < n_streams; ++k) {
            auto &sc = v_sc[k];
            CUDA_CHECK(
                cudaStreamCreateWithFlags(&sc.stream, cudaStreamNonBlocking));
            CUDA_CHECK(cudaEventCreate(&sc.kernel_start));
            CUDA_CHECK(cudaEventCreate(&sc.kernel_stop));
            CUDA_CHECK(cudaMalloc(&sc.d_frame, m_image_bytes));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_mean,
                                  m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(
                cudaMalloc(&sc.d_pd_sum, m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_sum2,
                                  m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(cudaMalloc(&sc.d_clusters, m_cluster_bytes));
            CUDA_CHECK(cudaMalloc(&sc.d_cluster_count, sizeof(uint32_t)));

            CUDA_CHECK(cudaMallocHost(reinterpret_cast<void **>(&sc.h_frame),
                                      m_image_bytes));
            CUDA_CHECK(
                cudaMallocHost(reinterpret_cast<void **>(&sc.h_cluster_count),
                               sizeof(uint32_t)));
            if (m_cluster_bytes > 0) {
                CUDA_CHECK(
                    cudaMallocHost(reinterpret_cast<void **>(&sc.h_clusters),
                                   m_cluster_bytes));
            }
        }
    }

    ~ClusterFinderCUDA() {
        for (auto &sc : v_sc) {
            if (sc.stream)
                cudaStreamSynchronize(sc.stream);

            if (sc.d_frame)
                cudaFree(sc.d_frame);
            if (sc.d_pd_mean)
                cudaFree(sc.d_pd_mean);
            if (sc.d_pd_sum)
                cudaFree(sc.d_pd_sum);
            if (sc.d_pd_sum2)
                cudaFree(sc.d_pd_sum2);
            if (sc.d_clusters)
                cudaFree(sc.d_clusters);
            if (sc.d_cluster_count)
                cudaFree(sc.d_cluster_count);

            if (sc.h_frame)
                cudaFreeHost(sc.h_frame);
            if (sc.h_clusters)
                cudaFreeHost(sc.h_clusters);
            if (sc.h_cluster_count)
                cudaFreeHost(sc.h_cluster_count);

            if (sc.kernel_start)
                cudaEventDestroy(sc.kernel_start);
            if (sc.kernel_stop)
                cudaEventDestroy(sc.kernel_stop);
            if (sc.stream)
                cudaStreamDestroy(sc.stream);
        }
    }

    // Non-copyable, non-movable
    ClusterFinderCUDA(const ClusterFinderCUDA &) = delete;
    ClusterFinderCUDA &operator=(const ClusterFinderCUDA &) = delete;
    ClusterFinderCUDA(ClusterFinderCUDA &&) = delete;
    ClusterFinderCUDA &operator=(ClusterFinderCUDA &&) = delete;

    void set_nSigma(COMPUTE_TYPE nSigma) { m_nSigma = nSigma; }
    COMPUTE_TYPE get_nSigma() const { return m_nSigma; }

    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        m_pedestal.push(frame);
        m_pedestal_dirty = true;
    }

    void clear_pedestal() {
        m_pedestal.clear();
        m_pedestal_dirty = true;
    }

    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }

    /**
     * @brief Move clusters out of the internal ClusterVector, optionally
     *        reallocating the internal one with the same capacity.
     */
    ClusterVector<ClusterType>
    steal_clusters(bool realloc_same_capacity = false) {
        ClusterVector<ClusterType> tmp = std::move(m_clusters);
        if (realloc_same_capacity)
            m_clusters = ClusterVector<ClusterType>(tmp.capacity());
        else
            m_clusters = ClusterVector<ClusterType>{};
        return tmp;
    }

    /**
     * @brief Find clusters in a single frame, appending them to the internal
     *        ClusterVector.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        if (m_pedestal_dirty) { // need to update the pedestal on the gpu
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }

        auto &sc = v_sc[0];
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        // First, CPU copies frame into a reusable pinned buffer
        std::memcpy(sc.h_frame, frame.data(), m_image_bytes);

        // Reset cluster counter
        CUDA_CHECK(cudaMemsetAsync(sc.d_cluster_count, 0, sizeof(uint32_t),
                                   sc.stream));

        // Upload frame
        CUDA_CHECK(cudaMemcpyAsync(sc.d_frame, sc.h_frame, m_image_bytes,
                                   cudaMemcpyHostToDevice, sc.stream));

        // Timed Kernel launch
        CUDA_CHECK(cudaEventRecord(sc.kernel_start, sc.stream));
        device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE,
                                              PEDESTAL_TYPE>
            <<<grid, block, shmem_bytes, sc.stream>>>(
                sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
                n_pd_samples, m_nSigma, nrows, ncols, sc.d_clusters,
                sc.d_cluster_count, static_cast<uint32_t>(m_capacity));
        CUDA_CHECK(cudaEventRecord(sc.kernel_stop, sc.stream));
        CUDA_CHECK(cudaGetLastError());

        // Read back cluster count into pinned buffer
        CUDA_CHECK(cudaMemcpyAsync(sc.h_cluster_count, sc.d_cluster_count,
                                   sizeof(uint32_t), cudaMemcpyDeviceToHost,
                                   sc.stream));

        // Synchronize to ensure count is available before the CPU reads
        // clusters
        CUDA_CHECK(cudaStreamSynchronize(sc.stream));

        record_kernel_time(sc);

        // Clamp to max in case of overflow
        uint32_t n_found = *sc.h_cluster_count;
        n_found = std::min(n_found, static_cast<uint32_t>(m_capacity));

        // Read back clusters
        m_clusters.set_frame_number(frame_number);
        if (n_found > 0) {
            append_device_clusters_to(m_clusters, sc, n_found);
        }
    }

    /**
     * @brief Batched cluster finding across multiple frames, using n_streams
     *        CUDA streams to overlap H2D transfer, kernel, and D2H transfer.
     *
     * Returns one ClusterVector per input frame (with frame_number set to
     * first_frame + i).
     */
    std::vector<ClusterVector<ClusterType>>
    find_clusters_batched(NDView<FRAME_TYPE, 3> frames,
                          uint64_t first_frame = 0) {
        if (m_pedestal_dirty) {
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }

        const size_t n_frames = frames.shape(0);
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames);
        for (size_t i = 0; i < n_frames; ++i) {
            results.emplace_back();
            results.back().set_frame_number(first_frame + i);
        }

        const size_t n_rounds = (n_frames + n_streams - 1) / n_streams;
        for (size_t round = 0; round < n_rounds; ++round) {

            // Launch phase: fan out kernels on all streams for this round
            for (int k = 0; k < n_streams; ++k) {
                // OOB guard
                const size_t frame_idx = round * n_streams + k;
                if (frame_idx >= n_frames)
                    continue;

                auto &sc_k = v_sc[k];
                const FRAME_TYPE *h_src =
                    frames.data() + frame_idx * m_image_size;

                std::memcpy(sc_k.h_frame, h_src, m_image_bytes);

                CUDA_CHECK(cudaMemsetAsync(sc_k.d_cluster_count, 0,
                                           sizeof(uint32_t), sc_k.stream));
                CUDA_CHECK(
                    cudaMemcpyAsync(sc_k.d_frame, sc_k.h_frame, m_image_bytes,
                                    cudaMemcpyHostToDevice, sc_k.stream));

                CUDA_CHECK(cudaEventRecord(sc_k.kernel_start, sc_k.stream));
                device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE,
                                                      PEDESTAL_TYPE>
                    <<<grid, block, shmem_bytes, sc_k.stream>>>(
                        sc_k.d_frame, sc_k.d_pd_mean, sc_k.d_pd_sum,
                        sc_k.d_pd_sum2, n_pd_samples, m_nSigma, nrows, ncols,
                        sc_k.d_clusters, sc_k.d_cluster_count,
                        static_cast<uint32_t>(m_capacity));
                CUDA_CHECK(cudaEventRecord(sc_k.kernel_stop, sc_k.stream));
                CUDA_CHECK(cudaGetLastError());

                // Queue count D2H immediately after the kernel
                CUDA_CHECK(cudaMemcpyAsync(
                    sc_k.h_cluster_count, sc_k.d_cluster_count,
                    sizeof(uint32_t), cudaMemcpyDeviceToHost, sc_k.stream));
            }

            // Drain phase: fan in results from all streams
            for (int k = 0; k < n_streams; ++k) {
                const size_t frame_idx = round * n_streams + k;
                if (frame_idx >= n_frames)
                    continue;

                auto &sc_k = v_sc[k];

                // Wait for memset -> H2D -> kernel -> count D2H
                CUDA_CHECK(cudaStreamSynchronize(sc_k.stream));

                record_kernel_time(sc_k);

                uint32_t n_found = *sc_k.h_cluster_count;
                n_found = std::min<uint32_t>(n_found,
                                             static_cast<uint32_t>(m_capacity));

                if (n_found > 0) {
                    append_device_clusters_to(results[frame_idx], sc_k,
                                              n_found);
                }
            }
        }

        return results;
    }

    float avg_kernel_time_ms() const {
        return m_frames_processed > 0 ? m_total_kernel_ms / m_frames_processed
                                      : 0.0f;
    }

    void reset_timers() {
        m_total_kernel_ms = 0.0f;
        m_frames_processed = 0;
    }

  private:
    /**
     * Upload the current host pedestal (mean, sum, sum2) to every stream's
     * device buffers. Called lazily before a find_clusters call when the
     * host pedestal has been updated.
     */
    void sync_pedestal_to_device() {
        // These return-by-value NDArrays must stay alive until the async
        // copies complete, so we synchronise at the end before they go out
        // of scope.
        NDArray<PEDESTAL_TYPE, 2> h_mean = m_pedestal.mean();
        NDArray<PEDESTAL_TYPE, 2> h_sum = m_pedestal.get_sum();
        NDArray<PEDESTAL_TYPE, 2> h_sum2 = m_pedestal.get_sum2();

        const size_t bytes = m_image_size * sizeof(PEDESTAL_TYPE);
        for (auto &sc : v_sc) {
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_mean, h_mean.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum, h_sum.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum2, h_sum2.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
        }
        for (auto &sc : v_sc)
            CUDA_CHECK(cudaStreamSynchronize(sc.stream));
    }

    /**
     * Copy n_found clusters from sc.d_clusters into the given ClusterVector
     * and block on the transfer.
     */
    void append_device_clusters_to(ClusterVector<ClusterType> &cv, SC &sc,
                                   uint32_t n_found) {

        CUDA_CHECK(cudaMemcpyAsync(sc.h_clusters, sc.d_clusters,
                                   n_found * sizeof(ClusterType),
                                   cudaMemcpyDeviceToHost, sc.stream));

        CUDA_CHECK(cudaStreamSynchronize(sc.stream));

        for (uint32_t i = 0; i < n_found; ++i)
            cv.push_back(sc.h_clusters[i]);
    }

    void record_kernel_time(SC &sc) {
        float ms = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&ms, sc.kernel_start, sc.kernel_stop));
        m_total_kernel_ms += ms;
        m_frames_processed++;
    }
};

} // namespace aare
