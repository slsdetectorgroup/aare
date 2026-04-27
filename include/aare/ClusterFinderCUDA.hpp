// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinder.hpp"
#include "aare/clusterfinder_kernel.cuh"
#include "aare/utils/cuda_check.cuh"
#include <cmath>
#include <cstdint>
#include <cstdio>
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

    COMPUTE_TYPE m_nSigma;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;
    bool m_pedestal_dirty = true;

    using SC = StreamContext<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    std::vector<SC> v_sc;

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

        v_sc.resize(n_streams);
        for (int k = 0; k < n_streams; ++k) {
            auto &sc = v_sc[k];
            CUDA_CHECK(cudaStreamCreate(&sc.stream));
            CUDA_CHECK(
                cudaMalloc(&sc.d_frame, m_image_size * sizeof(FRAME_TYPE)));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_mean,
                                  m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(
                cudaMalloc(&sc.d_pd_sum, m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_sum2,
                                  m_image_size * sizeof(PEDESTAL_TYPE)));
            CUDA_CHECK(
                cudaMalloc(&sc.d_clusters, capacity * sizeof(ClusterType)));
            CUDA_CHECK(cudaMalloc(&sc.d_cluster_count, sizeof(uint32_t)));
        }
    }

    ~ClusterFinderCUDA() {
        for (auto &sc : v_sc) {
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
        const size_t image_bytes = m_image_size * sizeof(FRAME_TYPE);
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        // Reset cluster counter
        CUDA_CHECK(cudaMemsetAsync(sc.d_cluster_count, 0, sizeof(uint32_t),
                                   sc.stream));

        // Upload frame
        CUDA_CHECK(cudaMemcpyAsync(sc.d_frame, frame.data(), image_bytes,
                                   cudaMemcpyHostToDevice, sc.stream));

        // Kernel launch
        device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE,
                                              PEDESTAL_TYPE>
            <<<grid, block, shmem_bytes, sc.stream>>>(
                sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
                n_pd_samples, m_nSigma, nrows, ncols, sc.d_clusters,
                sc.d_cluster_count, static_cast<uint32_t>(m_capacity));
        CUDA_CHECK(cudaGetLastError());

        // Read back cluster count
        uint32_t n_found = 0;
        CUDA_CHECK(cudaMemcpyAsync(&n_found, sc.d_cluster_count,
                                   sizeof(uint32_t), cudaMemcpyDeviceToHost,
                                   sc.stream));

        // Synchronize to ensure count is available before the CPU reads
        // clusters
        CUDA_CHECK(cudaStreamSynchronize(sc.stream));

        // Clamp to max in case of overflow
        if (n_found > m_capacity)
            n_found = m_capacity;

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
        const size_t image_bytes = m_image_size * sizeof(FRAME_TYPE);
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames);
        for (size_t i = 0; i < n_frames; ++i) {
            results.emplace_back(m_capacity);
            results.back().set_frame_number(first_frame + i);
        }

        std::vector<uint32_t> host_counts(n_streams, 0);

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

                CUDA_CHECK(cudaMemsetAsync(sc_k.d_cluster_count, 0,
                                           sizeof(uint32_t), sc_k.stream));
                CUDA_CHECK(cudaMemcpyAsync(sc_k.d_frame, h_src, image_bytes,
                                           cudaMemcpyHostToDevice,
                                           sc_k.stream));

                device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE,
                                                      PEDESTAL_TYPE>
                    <<<grid, block, shmem_bytes, sc_k.stream>>>(
                        sc_k.d_frame, sc_k.d_pd_mean, sc_k.d_pd_sum,
                        sc_k.d_pd_sum2, n_pd_samples, m_nSigma, nrows, ncols,
                        sc_k.d_clusters, sc_k.d_cluster_count, m_capacity);
                CUDA_CHECK(cudaGetLastError());
            }

            // Drain phase: fan in results from all streams
            for (int k = 0; k < n_streams; ++k) {
                const size_t frame_idx = round * n_streams + k;
                if (frame_idx >= n_frames)
                    continue;

                auto &sc_k = v_sc[k];

                CUDA_CHECK(cudaMemcpyAsync(
                    &host_counts[k], sc_k.d_cluster_count, sizeof(uint32_t),
                    cudaMemcpyDeviceToHost, sc_k.stream));
                CUDA_CHECK(cudaStreamSynchronize(sc_k.stream));

                uint32_t n_found = host_counts[k];
                if (n_found > m_capacity)
                    n_found = m_capacity;

                if (n_found > 0) {
                    append_device_clusters_to(results[frame_idx], sc_k,
                                              n_found);
                }
            }
        }

        return results;
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
        std::vector<ClusterType> staging(n_found);
        CUDA_CHECK(cudaMemcpyAsync(staging.data(), sc.d_clusters,
                                   n_found * sizeof(ClusterType),
                                   cudaMemcpyDeviceToHost, sc.stream));
        CUDA_CHECK(cudaStreamSynchronize(sc.stream));
        for (const auto &c : staging)
            cv.push_back(c);
    }
};

} // namespace aare
