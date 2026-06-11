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
#include <vector>

namespace aare {

// Per-stream device resources (device-side only; all pinned host staging is
// class-level)
template <typename ClusterType, typename FRAME_TYPE, typename PEDESTAL_TYPE>
struct StreamContext {
    cudaStream_t stream = nullptr;
    FRAME_TYPE *d_frame = nullptr;
    float *d_pd_mean = nullptr; // always float on device; host stays double
    float *d_pd_sum = nullptr;
    float *d_pd_sum2 = nullptr;
    uint8_t *d_output = nullptr; // [uint32_t count | ClusterType clusters[max]]
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

    // Two ping-pong output slots. At most 2 batches can be in flight at once:
    // the caller submits batch B before collecting batch A, so A and B occupy
    // separate slots. A third submit() without an intervening collect() throws.
    static constexpr int NUM_SLOTS = 2;

    void *h_output_slots[NUM_SLOTS] = {nullptr, nullptr};
    size_t m_output_slot_capacity[NUM_SLOTS] = {0, 0};

    // Per-slot state consumed by collect()
    bool m_slot_in_flight[NUM_SLOTS] = {false, false};
    size_t m_slot_n_frames[NUM_SLOTS] = {0, 0};
    uint64_t m_slot_first_frame[NUM_SLOTS] = {0, 0};
    int m_slot_streams_used[NUM_SLOTS] = {0, 0};
    int m_next_slot = 0;

    // Pointer registered via cudaHostRegister by the caller (for pinned H2D
    // speed). The class tracks it only to unregister in the destructor if the
    // caller forgets.
    void *m_registered_input = nullptr;

    Shape<2> m_shape;
    size_t nrows;
    size_t ncols;
    size_t m_image_size; // nrows * ncols
    size_t m_image_bytes;

    int n_streams;
    size_t m_max_clusters_per_frame;

    // Per-frame output layout helpers
    size_t m_output_bytes_per_frame; // sizeof(uint32_t) + max *
                                     // sizeof(ClusterType), aligned
    size_t m_clusters_offset; // offset of cluster array within output block

    COMPUTE_TYPE m_nSigma;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;
    bool m_pedestal_dirty = true;

    using SC = StreamContext<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    std::vector<SC> v_sc;

    float m_total_kernel_ms = 0.0f;
    size_t m_frames_processed = 0;

    // Per-slot kernel timing event pools (sized lazily to the largest batch)
    std::vector<cudaEvent_t> m_kernel_start_pools[NUM_SLOTS];
    std::vector<cudaEvent_t> m_kernel_stop_pools[NUM_SLOTS];

    // Per-slot, per-stream "batch done" sync events (timing disabled).
    // Recorded after the last D2H of each submit_batch(). collect() waits on
    // these via cudaEventSynchronize so it unblocks as soon as the batch
    // finishes, even if the next batch is already queued in the same streams.
    std::vector<cudaEvent_t> m_batch_done[NUM_SLOTS];

    // Kernel parameters
    dim3 grid;
    dim3 block;
    size_t shmem_bytes;

  public:
    /**
     * @brief Opaque handle returned by submit_batch(). Pass to collect().
     */
    struct BatchToken {
        int slot;
    };

    /**
     * @brief Construct a ClusterFinderCUDA
     *
     * @param shape_                    shape of the detector frame (rows, cols)
     * @param nSigma                    threshold in units of per-pixel pedestal
     * std
     * @param max_clusters_per_frame    tight upper bound on clusters/frame for
     * fixed-size D2H
     * @param n_streams_                number of CUDA streams for multi-frame
     * overlap
     */
    ClusterFinderCUDA(Shape<2> shape_, COMPUTE_TYPE nSigma = 5.0,
                      size_t max_clusters_per_frame = 2048, int n_streams_ = 5)
        : m_shape(shape_), nrows(shape_[0]), ncols(shape_[1]),
          m_image_size(nrows * ncols), n_streams(n_streams_),
          m_max_clusters_per_frame(max_clusters_per_frame), m_nSigma(nSigma),
          m_pedestal(shape_[0], shape_[1]), m_clusters(max_clusters_per_frame) {
        if (n_streams_ <= 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: n_streams must be > 0");
        }

        if (max_clusters_per_frame >
            static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: max_clusters_per_frame must fit in "
                "uint32_t");
        }

        if (max_clusters_per_frame == 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDA: max_clusters_per_frame must be > 0");
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

        // Output block layout: [count][padding to ClusterType
        // alignment][clusters]
        constexpr size_t cluster_align = alignof(ClusterType);
        const size_t count_bytes = sizeof(uint32_t);
        // next multiple of cluster_align
        m_clusters_offset =
            (count_bytes + cluster_align - 1) & ~(cluster_align - 1);
        m_output_bytes_per_frame =
            m_clusters_offset + m_max_clusters_per_frame * sizeof(ClusterType);

        v_sc.resize(n_streams);
        for (int k = 0; k < n_streams; ++k) {
            auto &sc = v_sc[k];
            CUDA_CHECK(
                cudaStreamCreateWithFlags(&sc.stream, cudaStreamNonBlocking));
            CUDA_CHECK(cudaMalloc(&sc.d_frame, m_image_bytes));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_mean, m_image_size * sizeof(float)));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_sum, m_image_size * sizeof(float)));
            CUDA_CHECK(cudaMalloc(&sc.d_pd_sum2, m_image_size * sizeof(float)));
            CUDA_CHECK(cudaMalloc(&sc.d_output, m_output_bytes_per_frame));
        }

        for (int s = 0; s < NUM_SLOTS; ++s) {
            m_batch_done[s].resize(n_streams);
            for (int k = 0; k < n_streams; ++k)
                CUDA_CHECK(cudaEventCreateWithFlags(&m_batch_done[s][k],
                                                    cudaEventDisableTiming));
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
            if (sc.d_output)
                cudaFree(sc.d_output);
            if (sc.stream)
                cudaStreamDestroy(sc.stream);
        }

        for (int s = 0; s < NUM_SLOTS; ++s) {
            for (auto e : m_kernel_start_pools[s])
                if (e)
                    cudaEventDestroy(e);
            for (auto e : m_kernel_stop_pools[s])
                if (e)
                    cudaEventDestroy(e);
            for (auto e : m_batch_done[s])
                if (e)
                    cudaEventDestroy(e);
            if (h_output_slots[s])
                cudaFreeHost(h_output_slots[s]);
        }

        if (m_registered_input)
            cudaHostUnregister(m_registered_input);
    }

    // Non-copyable, non-movable
    ClusterFinderCUDA(const ClusterFinderCUDA &) = delete;
    ClusterFinderCUDA &operator=(const ClusterFinderCUDA &) = delete;
    ClusterFinderCUDA(ClusterFinderCUDA &&) = delete;
    ClusterFinderCUDA &operator=(ClusterFinderCUDA &&) = delete;

    void set_nSigma(COMPUTE_TYPE nSigma) { m_nSigma = nSigma; }
    COMPUTE_TYPE get_nSigma() const { return m_nSigma; }

    /**
     * @brief Pin an existing host buffer so that find_clusters_batched
     * transfers it at full PCIe bandwidth (~22 GB/s) instead of going through
     * the CUDA driver's internal staging (~15 GB/s for pageable memory).
     *
     * Call once before the processing loop (not per-frame). The buffer must
     * cover the largest NDView you will pass to find_clusters_batched.
     * Call unregister_input_buffer() when done, or the destructor will clean
     * up.
     */
    void register_input_buffer(void *ptr, size_t bytes) {
        if (m_registered_input)
            CUDA_CHECK(cudaHostUnregister(m_registered_input));
        CUDA_CHECK(cudaHostRegister(ptr, bytes, cudaHostRegisterDefault));
        m_registered_input = ptr;
    }

    void unregister_input_buffer() {
        if (m_registered_input) {
            CUDA_CHECK(cudaHostUnregister(m_registered_input));
            m_registered_input = nullptr;
        }
    }

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
     * @brief Find clusters in a single frame, appending results to the internal
     *        ClusterVector (accessible via steal_clusters).
     *        Delegates to find_clusters_batched to avoid duplicating the GPU
     * pipeline.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        NDView<FRAME_TYPE, 3> batch(
            frame.data(),
            {1, static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)});
        auto results = find_clusters_batched(batch, frame_number);
        m_clusters.set_frame_number(frame_number);
        auto &cv = results[0];
        for (size_t i = 0; i < cv.size(); ++i)
            m_clusters.push_back(cv[i]);
    }

    /**
     * @brief Enqueue one batch of frames onto the GPU without waiting for
     *        completion. Returns a BatchToken to pass to collect().
     *
     * At most NUM_SLOTS (2) batches can be in flight simultaneously. Submitting
     * a third batch before collecting one throws std::runtime_error.
     *
     * Typical pipeline usage to eliminate inter-batch GPU idle time:
     *
     *   auto tok = cf.submit_batch(frames_a, 0);
     *   for (size_t b = 1; b < n_batches; ++b) {
     *       auto next = cf.submit_batch(frames_b, b * N); // enqueue B while
     *                                                      // GPU still runs A
     *       process(cf.collect(tok));  // drain A; GPU runs B concurrently
     *       tok = next;
     *   }
     *   process(cf.collect(tok));  // drain final batch
     */
    BatchToken submit_batch(NDView<FRAME_TYPE, 3> frames,
                            uint64_t first_frame = 0) {
        if (m_pedestal_dirty) {
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }

        const int slot = m_next_slot;
        if (m_slot_in_flight[slot])
            throw std::runtime_error(
                "ClusterFinderCUDA: both batch slots are in flight — call "
                "collect() before submitting a third batch");
        m_next_slot = 1 - slot;

        const size_t n_frames_batch = static_cast<size_t>(frames.shape(0));
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        // Grow pinned D2H output buffer for this slot if needed
        if (n_frames_batch > m_output_slot_capacity[slot]) {
            if (h_output_slots[slot])
                CUDA_CHECK(cudaFreeHost(h_output_slots[slot]));
            CUDA_CHECK(
                cudaMallocHost(&h_output_slots[slot],
                               n_frames_batch * m_output_bytes_per_frame));
            m_output_slot_capacity[slot] = n_frames_batch;
        }

        ensure_event_pool(slot, n_frames_batch);

        // Launch all frames round-robin across streams
        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            auto &sc = v_sc[frame_idx % n_streams];

            const FRAME_TYPE *h_src = frames.data() + frame_idx * m_image_size;
            auto *d_cluster_count = reinterpret_cast<uint32_t *>(sc.d_output);

            CUDA_CHECK(cudaMemsetAsync(d_cluster_count, 0, sizeof(uint32_t),
                                       sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_frame, h_src, m_image_bytes,
                                       cudaMemcpyHostToDevice, sc.stream));

            auto *d_clusters = reinterpret_cast<ClusterType *>(
                sc.d_output + m_clusters_offset);
            CUDA_CHECK(cudaEventRecord(m_kernel_start_pools[slot][frame_idx],
                                       sc.stream));
            device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE>
                <<<grid, block, shmem_bytes, sc.stream>>>(
                    sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
                    n_pd_samples, m_nSigma, nrows, ncols, d_clusters,
                    d_cluster_count,
                    static_cast<uint32_t>(m_max_clusters_per_frame));
            CUDA_CHECK(cudaEventRecord(m_kernel_stop_pools[slot][frame_idx],
                                       sc.stream));
            CUDA_CHECK(cudaGetLastError());

            void *h_out = static_cast<char *>(h_output_slots[slot]) +
                          frame_idx * m_output_bytes_per_frame;
            CUDA_CHECK(cudaMemcpyAsync(h_out, sc.d_output,
                                       m_output_bytes_per_frame,
                                       cudaMemcpyDeviceToHost, sc.stream));
        }

        // Record per-stream "batch done" events after all D2H for this batch.
        // collect() waits on these via cudaEventSynchronize, which unblocks as
        // soon as this batch's last D2H completes — even if the next batch is
        // already queued behind it in the same streams.
        const int streams_used =
            std::min<int>(n_streams, static_cast<int>(n_frames_batch));
        for (int k = 0; k < streams_used; ++k)
            CUDA_CHECK(cudaEventRecord(m_batch_done[slot][k], v_sc[k].stream));

        m_slot_n_frames[slot] = n_frames_batch;
        m_slot_first_frame[slot] = first_frame;
        m_slot_streams_used[slot] = streams_used;
        m_slot_in_flight[slot] = true;

        return BatchToken{slot};
    }

    /**
     * @brief Wait for a previously submitted batch and return its results.
     *
     * Uses cudaEventSynchronize (not cudaStreamSynchronize) so that a
     * concurrently running batch already queued in the same streams is not
     * waited on — the GPU keeps running while the CPU drains this batch.
     */
    std::vector<ClusterVector<ClusterType>> collect(BatchToken token) {
        const int slot = token.slot;
        if (!m_slot_in_flight[slot])
            throw std::runtime_error(
                "ClusterFinderCUDA: collect() called on a slot that is not "
                "in flight");

        const size_t n_frames_batch = m_slot_n_frames[slot];
        const uint64_t first_frame = m_slot_first_frame[slot];
        const int streams_used = m_slot_streams_used[slot];

        // Wait until this batch's D2H is complete in every stream it used.
        // cudaEventSynchronize returns as soon as the event fires, without
        // waiting for any later operations already queued in the stream.
        for (int k = 0; k < streams_used; ++k)
            CUDA_CHECK(cudaEventSynchronize(m_batch_done[slot][k]));

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames_batch);
        for (size_t i = 0; i < n_frames_batch; ++i) {
            results.emplace_back();
            results.back().set_frame_number(first_frame + i);
        }

        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            const void *h_out =
                static_cast<const char *>(h_output_slots[slot]) +
                frame_idx * m_output_bytes_per_frame;
            uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
            n_found = std::min<uint32_t>(
                n_found, static_cast<uint32_t>(m_max_clusters_per_frame));

            if (n_found > 0) {
                const auto *src = reinterpret_cast<const ClusterType *>(
                    static_cast<const char *>(h_out) + m_clusters_offset);
                results[frame_idx].resize(n_found);
                std::memcpy(results[frame_idx].data(), src,
                            n_found * sizeof(ClusterType));
            }

            float kernel_ms = 0.0f;
            CUDA_CHECK(cudaEventElapsedTime(
                &kernel_ms, m_kernel_start_pools[slot][frame_idx],
                m_kernel_stop_pools[slot][frame_idx]));
            m_total_kernel_ms += kernel_ms;
        }

        m_frames_processed += n_frames_batch;
        m_slot_in_flight[slot] = false;
        return results;
    }

    /**
     * @brief Synchronous batched cluster finding across multiple frames, using
     *        n_streams CUDA streams to overlap H2D, kernel, and D2H.
     *
     * Returns one ClusterVector per input frame (with frame_number set to
     * first_frame + i). Does not go through submit_batch/collect so it carries
     * no async-slot overhead.
     */
    std::vector<ClusterVector<ClusterType>>
    find_clusters_batched(NDView<FRAME_TYPE, 3> frames,
                          uint64_t first_frame = 0) {
        if (m_pedestal_dirty) {
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }

        const size_t n_frames_batch = static_cast<size_t>(frames.shape(0));
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        // Lazy grow D2H output staging buffer (one slot per frame)
        if (n_frames_batch > m_output_slot_capacity[0]) {
            if (h_output_slots[0])
                CUDA_CHECK(cudaFreeHost(h_output_slots[0]));
            CUDA_CHECK(cudaMallocHost(
                &h_output_slots[0], n_frames_batch * m_output_bytes_per_frame));
            m_output_slot_capacity[0] = n_frames_batch;
        }

        ensure_event_pool(0, n_frames_batch);

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames_batch);
        for (size_t i = 0; i < n_frames_batch; ++i) {
            results.emplace_back();
            results.back().set_frame_number(first_frame + i);
        }

        // Launch all frames round-robin across streams.
        // If the caller has called register_input_buffer() on frames.data(),
        // H2D runs at pinned DMA bandwidth (~22 GB/s); otherwise the CUDA
        // driver stages it internally (~15 GB/s for pageable memory).
        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            auto &sc = v_sc[frame_idx % n_streams];

            const FRAME_TYPE *h_src = frames.data() + frame_idx * m_image_size;
            auto *d_cluster_count = reinterpret_cast<uint32_t *>(sc.d_output);

            CUDA_CHECK(cudaMemsetAsync(d_cluster_count, 0, sizeof(uint32_t),
                                       sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_frame, h_src, m_image_bytes,
                                       cudaMemcpyHostToDevice, sc.stream));

            auto *d_clusters = reinterpret_cast<ClusterType *>(
                sc.d_output + m_clusters_offset);
            CUDA_CHECK(
                cudaEventRecord(m_kernel_start_pools[0][frame_idx], sc.stream));
            device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE>
                <<<grid, block, shmem_bytes, sc.stream>>>(
                    sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
                    n_pd_samples, m_nSigma, nrows, ncols, d_clusters,
                    d_cluster_count,
                    static_cast<uint32_t>(m_max_clusters_per_frame));
            CUDA_CHECK(
                cudaEventRecord(m_kernel_stop_pools[0][frame_idx], sc.stream));
            CUDA_CHECK(cudaGetLastError());

            void *h_out = static_cast<char *>(h_output_slots[0]) +
                          frame_idx * m_output_bytes_per_frame;
            CUDA_CHECK(cudaMemcpyAsync(h_out, sc.d_output,
                                       m_output_bytes_per_frame,
                                       cudaMemcpyDeviceToHost, sc.stream));
        }

        const int streams_used =
            std::min<int>(n_streams, static_cast<int>(n_frames_batch));
        for (int k = 0; k < streams_used; ++k)
            CUDA_CHECK(cudaStreamSynchronize(v_sc[k].stream));

        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            const void *h_out = static_cast<const char *>(h_output_slots[0]) +
                                frame_idx * m_output_bytes_per_frame;
            uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
            n_found = std::min<uint32_t>(
                n_found, static_cast<uint32_t>(m_max_clusters_per_frame));

            if (n_found > 0) {
                const auto *src = reinterpret_cast<const ClusterType *>(
                    static_cast<const char *>(h_out) + m_clusters_offset);
                results[frame_idx].resize(n_found);
                std::memcpy(results[frame_idx].data(), src,
                            n_found * sizeof(ClusterType));
            }

            float kernel_ms = 0.0f;
            CUDA_CHECK(cudaEventElapsedTime(&kernel_ms,
                                            m_kernel_start_pools[0][frame_idx],
                                            m_kernel_stop_pools[0][frame_idx]));
            m_total_kernel_ms += kernel_ms;
        }

        m_frames_processed += n_frames_batch;
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
        NDArray<PEDESTAL_TYPE, 2> h_mean = m_pedestal.mean();
        NDArray<PEDESTAL_TYPE, 2> h_sum = m_pedestal.get_sum();
        NDArray<PEDESTAL_TYPE, 2> h_sum2 = m_pedestal.get_sum2();

        // Host accumulates in double for precision; cast to float for device
        // to halve global-memory bandwidth and eliminate FP64 arithmetic.
        std::vector<float> f_mean(m_image_size);
        std::vector<float> f_sum(m_image_size);
        std::vector<float> f_sum2(m_image_size);
        for (size_t i = 0; i < m_image_size; ++i) {
            f_mean[i] = static_cast<float>(h_mean.data()[i]);
            f_sum[i] = static_cast<float>(h_sum.data()[i]);
            f_sum2[i] = static_cast<float>(h_sum2.data()[i]);
        }

        const size_t bytes = m_image_size * sizeof(float);
        for (auto &sc : v_sc) {
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_mean, f_mean.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum, f_sum.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum2, f_sum2.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
        }
        for (auto &sc : v_sc)
            CUDA_CHECK(cudaStreamSynchronize(sc.stream));
    }

    void ensure_event_pool(int slot, size_t n_frames) {
        const size_t old_size = m_kernel_start_pools[slot].size();
        if (n_frames <= old_size)
            return;
        m_kernel_start_pools[slot].resize(n_frames);
        m_kernel_stop_pools[slot].resize(n_frames);
        for (size_t i = old_size; i < n_frames; ++i) {
            CUDA_CHECK(cudaEventCreate(&m_kernel_start_pools[slot][i]));
            CUDA_CHECK(cudaEventCreate(&m_kernel_stop_pools[slot][i]));
        }
    }
};

} // namespace aare
