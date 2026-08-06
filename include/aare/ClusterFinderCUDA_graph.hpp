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

// Per-stream device resources and CUDA Graph state.
template <typename ClusterType, typename FRAME_TYPE, typename PEDESTAL_TYPE>
struct StreamContextGraph {
    cudaStream_t stream = nullptr;
    FRAME_TYPE *d_frame = nullptr;
    // Device pedestal precision is set by DEVICE_PED_TYPE in the kernel header.
    // Accumulators hold CENTERED moments of Y = X - d_pd_off (see kernel).
    device::DEVICE_PED_TYPE *d_pd_mean = nullptr;
    device::DEVICE_PED_TYPE *d_pd_sum = nullptr;
    device::DEVICE_PED_TYPE *d_pd_sum2 = nullptr;
    device::DEVICE_PED_TYPE *d_pd_off = nullptr; // frozen per-pixel baseline X0
    uint8_t *d_output = nullptr; // [uint32_t count | ClusterType clusters[max]]

    // CUDA Graph handles — rebuilt on pedestal change or h_output_pinned resize
    cudaGraph_t graph = nullptr;
    cudaGraphExec_t graphExec = nullptr;
    cudaGraphNode_t h2d_node = nullptr;
    cudaGraphNode_t d2h_node = nullptr;

    // Kernel argument storage for cudaKernelNodeParams.
    // kargs_ptrs holds addresses into these fields; members must not move.
    FRAME_TYPE *karg_d_frame = nullptr;
    device::DEVICE_PED_TYPE *karg_d_pd_mean = nullptr;
    device::DEVICE_PED_TYPE *karg_d_pd_sum = nullptr;
    device::DEVICE_PED_TYPE *karg_d_pd_sum2 = nullptr;
    device::DEVICE_PED_TYPE *karg_d_pd_off = nullptr;
    uint32_t karg_n_pd_samples = 0;
    device::COMPUTE_TYPE karg_nSigma = 0.0f;
    size_t karg_nrows = 0;
    size_t karg_ncols = 0;
    ClusterType *karg_d_clusters = nullptr;
    uint32_t *karg_d_cluster_count = nullptr;
    uint32_t karg_max_clusters = 0;
    void *kargs_ptrs[12] = {};

    // Per-frame update templates for H2D and D2H memcpy nodes
    cudaMemcpy3DParms h2d_params = {};
    cudaMemcpy3DParms d2h_params = {};
};

template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinderCUDAGraph {
    using COMPUTE_TYPE =
        device::COMPUTE_TYPE; // match the kernel's internal precision

    static constexpr int BLOCK_X = 16;
    static constexpr int BLOCK_Y = 16;
    static constexpr int col_radius = ClusterType::cluster_size_x / 2;
    static constexpr int row_radius = ClusterType::cluster_size_y / 2;

    size_t m_output_pinned_capacity =
        0; // # frames currently allocated in h_output_pinned
    void *h_output_pinned = nullptr;

    // Pointer registered via cudaHostRegister by the caller (for pinned H2D
    // speed). The class tracks it only to unregister in the destructor if the
    // caller forgets.
    void *m_registered_input = nullptr;

    // Pinned scratch buffer used as placeholder source/destination when
    // building CUDA Graph nodes (before per-frame pointers are known).
    void *m_placeholder = nullptr;

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
    bool m_graphs_dirty = true; // set when pedestal or h_output_pinned changes
    // Frozen per-pixel baseline X0 (~mean at t=0), captured once on first sync.
    std::vector<device::DEVICE_PED_TYPE> m_offset;

    using SC = StreamContextGraph<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    std::vector<SC> v_sc;

    size_t m_frames_processed = 0;

    // Kernel parameters
    dim3 grid;
    dim3 block;
    size_t shmem_bytes;

  public:
    /**
     * @brief Construct a ClusterFinderCUDAGraph
     *
     * @param shape_                    shape of the detector frame (rows, cols)
     * @param nSigma                    threshold in units of per-pixel pedestal
     * std
     * @param max_clusters_per_frame    tight upper bound on clusters/frame for
     * fixed-size D2H
     * @param n_streams_                number of CUDA streams for multi-frame
     * overlap
     */
    ClusterFinderCUDAGraph(Shape<2> shape_, COMPUTE_TYPE nSigma = 5.0,
                           size_t max_clusters_per_frame = 2048,
                           int n_streams_ = 4)
        : m_shape(shape_), nrows(shape_[0]), ncols(shape_[1]),
          m_image_size(nrows * ncols), n_streams(n_streams_),
          m_max_clusters_per_frame(max_clusters_per_frame), m_nSigma(nSigma),
          m_pedestal(shape_[0], shape_[1]), m_clusters(max_clusters_per_frame) {
        if (n_streams_ <= 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDAGraph: n_streams must be > 0");
        }

        if (max_clusters_per_frame >
            static_cast<size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::invalid_argument(
                "ClusterFinderCUDAGraph: max_clusters_per_frame must fit in "
                "uint32_t");
        }

        if (max_clusters_per_frame == 0) {
            throw std::invalid_argument(
                "ClusterFinderCUDAGraph: max_clusters_per_frame must be > 0");
        }

        // Grid/Block dimensions
        block = dim3(BLOCK_X, BLOCK_Y);
        grid = dim3((static_cast<unsigned int>(ncols) + BLOCK_X - 1) / BLOCK_X,
                    (static_cast<unsigned int>(nrows) + BLOCK_Y - 1) / BLOCK_Y);

        // Shared memory: one tile of (BLOCK_X + 2*col_radius) x (BLOCK_Y +
        // 2*row_radius) elements, sized in COMPUTE_TYPE (the kernel's stencil
        // precision), not PEDESTAL_TYPE.
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

        // Pinned scratch for CUDA Graph node placeholders
        CUDA_CHECK(cudaMallocHost(
            &m_placeholder, std::max(m_image_bytes, m_output_bytes_per_frame)));

        v_sc.resize(n_streams);
        for (int k = 0; k < n_streams; ++k) {
            auto &sc = v_sc[k];
            CUDA_CHECK(
                cudaStreamCreateWithFlags(&sc.stream, cudaStreamNonBlocking));
            CUDA_CHECK(cudaMalloc(&sc.d_frame, m_image_bytes));
            CUDA_CHECK(cudaMalloc(
                &sc.d_pd_mean, m_image_size * sizeof(device::DEVICE_PED_TYPE)));
            CUDA_CHECK(cudaMalloc(
                &sc.d_pd_sum, m_image_size * sizeof(device::DEVICE_PED_TYPE)));
            CUDA_CHECK(cudaMalloc(
                &sc.d_pd_sum2, m_image_size * sizeof(device::DEVICE_PED_TYPE)));
            CUDA_CHECK(cudaMalloc(
                &sc.d_pd_off, m_image_size * sizeof(device::DEVICE_PED_TYPE)));
            CUDA_CHECK(cudaMalloc(&sc.d_output, m_output_bytes_per_frame));
        }
    }

    ~ClusterFinderCUDAGraph() {
        for (auto &sc : v_sc) {
            if (sc.stream)
                cudaStreamSynchronize(sc.stream);
            if (sc.graphExec)
                cudaGraphExecDestroy(sc.graphExec);
            if (sc.graph)
                cudaGraphDestroy(sc.graph);
            if (sc.d_frame)
                cudaFree(sc.d_frame);
            if (sc.d_pd_mean)
                cudaFree(sc.d_pd_mean);
            if (sc.d_pd_sum)
                cudaFree(sc.d_pd_sum);
            if (sc.d_pd_sum2)
                cudaFree(sc.d_pd_sum2);
            if (sc.d_pd_off)
                cudaFree(sc.d_pd_off);
            if (sc.d_output)
                cudaFree(sc.d_output);
            if (sc.stream)
                cudaStreamDestroy(sc.stream);
        }

        if (h_output_pinned)
            cudaFreeHost(h_output_pinned);
        if (m_registered_input)
            cudaHostUnregister(m_registered_input);
        if (m_placeholder)
            cudaFreeHost(m_placeholder);
    }

    // Non-copyable, non-movable
    ClusterFinderCUDAGraph(const ClusterFinderCUDAGraph &) = delete;
    ClusterFinderCUDAGraph &operator=(const ClusterFinderCUDAGraph &) = delete;
    ClusterFinderCUDAGraph(ClusterFinderCUDAGraph &&) = delete;
    ClusterFinderCUDAGraph &operator=(ClusterFinderCUDAGraph &&) = delete;

    void set_nSigma(COMPUTE_TYPE nSigma) {
        m_nSigma = nSigma;
        m_graphs_dirty = true;
    }
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
        m_offset.clear(); // re-capture the baseline on the next sync
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

        const size_t n_frames_batch = frames.shape(0);
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        // Lazy grow D2H output staging buffer (one slot per frame)
        if (n_frames_batch > m_output_pinned_capacity) {
            if (h_output_pinned)
                CUDA_CHECK(cudaFreeHost(h_output_pinned));
            CUDA_CHECK(cudaMallocHost(
                &h_output_pinned, n_frames_batch * m_output_bytes_per_frame));
            m_output_pinned_capacity = n_frames_batch;
            m_graphs_dirty = true;
        }

        // Rebuild graphs after pedestal sync or h_output_pinned resize.
        // h_output_pinned must be allocated first (used as valid host memory
        // for the D2H placeholder during graph construction).
        if (m_graphs_dirty) {
            build_graphs(n_pd_samples);
            m_graphs_dirty = false;
        }

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames_batch);
        for (size_t i = 0; i < n_frames_batch; ++i) {
            results.emplace_back();
            results.back().set_frame_number(first_frame + i);
        }

        // Per-frame: update only the two pointer-valued nodes in the exec,
        // then launch the graph. cudaGraphExecMemcpyNodeSetParams takes effect
        // on the next launch and does not affect in-flight executions.
        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            auto &sc = v_sc[frame_idx % n_streams];

            const FRAME_TYPE *h_src = frames.data() + frame_idx * m_image_size;
            void *h_slot = static_cast<char *>(h_output_pinned) +
                           frame_idx * m_output_bytes_per_frame;

            sc.h2d_params.srcPtr =
                make_cudaPitchedPtr(const_cast<FRAME_TYPE *>(h_src),
                                    m_image_bytes, m_image_bytes, 1);
            CUDA_CHECK(cudaGraphExecMemcpyNodeSetParams(
                sc.graphExec, sc.h2d_node, &sc.h2d_params));

            sc.d2h_params.dstPtr = make_cudaPitchedPtr(
                h_slot, m_output_bytes_per_frame, m_output_bytes_per_frame, 1);
            CUDA_CHECK(cudaGraphExecMemcpyNodeSetParams(
                sc.graphExec, sc.d2h_node, &sc.d2h_params));

            CUDA_CHECK(cudaGraphLaunch(sc.graphExec, sc.stream));
        }

        // Sync once per stream
        const int streams_used =
            std::min<int>(n_streams, static_cast<int>(n_frames_batch));
        for (int k = 0; k < streams_used; ++k)
            CUDA_CHECK(cudaStreamSynchronize(v_sc[k].stream));

        // Drain: fan in results from pinned D2H output pool
        for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
            const void *h_slot = static_cast<const char *>(h_output_pinned) +
                                 frame_idx * m_output_bytes_per_frame;
            uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_slot);
            n_found = std::min<uint32_t>(
                n_found, static_cast<uint32_t>(m_max_clusters_per_frame));

            if (n_found > 0) {
                const auto *src = reinterpret_cast<const ClusterType *>(
                    static_cast<const char *>(h_slot) + m_clusters_offset);
                results[frame_idx].resize(n_found);
                std::memcpy(results[frame_idx].data(), src,
                            n_found * sizeof(ClusterType));
            }
        }

        m_frames_processed += n_frames_batch;
        return results;
    }

    float avg_kernel_time_ms() const { return 0.0f; }

    void reset_timers() { m_frames_processed = 0; }

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

        using DPT = device::DEVICE_PED_TYPE;
        const double n = static_cast<double>(m_pedestal.n_samples());

        // Capture the frozen per-pixel baseline X0 (≈ mean at t=0) ONCE, then
        // upload CENTERED accumulators (Y = X − X0) so the device variance
        // avoids f32 catastrophic cancellation. See ClusterFinderCUDA.hpp for
        // the full rationale; the centering is done in double, then cast to
        // DPT.
        if (m_offset.size() != m_image_size) {
            m_offset.resize(m_image_size);
            for (size_t i = 0; i < m_image_size; ++i)
                m_offset[i] = static_cast<DPT>(std::llround(h_mean.data()[i]));
        }

        std::vector<DPT> f_mean(m_image_size);
        std::vector<DPT> f_sum(m_image_size);
        std::vector<DPT> f_sum2(m_image_size);
        for (size_t i = 0; i < m_image_size; ++i) {
            const double X0 = static_cast<double>(m_offset[i]);
            const double s = h_sum.data()[i];
            const double s2 = h_sum2.data()[i];
            f_mean[i] = static_cast<DPT>(h_mean.data()[i]);
            f_sum[i] = static_cast<DPT>(s - n * X0);
            f_sum2[i] = static_cast<DPT>(s2 - 2.0 * X0 * s + n * X0 * X0);
        }

        const size_t bytes = m_image_size * sizeof(DPT);
        for (auto &sc : v_sc) {
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_mean, f_mean.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum, f_sum.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_sum2, f_sum2.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
            CUDA_CHECK(cudaMemcpyAsync(sc.d_pd_off, m_offset.data(), bytes,
                                       cudaMemcpyHostToDevice, sc.stream));
        }
        for (auto &sc : v_sc)
            CUDA_CHECK(cudaStreamSynchronize(sc.stream));

        // Kernel's n_pd_samples and the pedestal arrays changed; graphs must
        // be rebuilt before the next launch.
        m_graphs_dirty = true;
    }

    /**
     * Build one CUDA Graph per stream: memset(count) + H2D(frame) → kernel →
     * D2H(output). Placeholder pointers are used for the H2D source and D2H
     * destination; per-frame pointers are injected via
     * cudaGraphExecMemcpyNodeSetParams in find_clusters_batched.
     *
     * Precondition: h_output_pinned must be allocated before calling this.
     */
    void build_graphs(uint32_t n_pd_samples) {
        for (int k = 0; k < n_streams; ++k) {
            auto &sc = v_sc[k];

            if (sc.graphExec) {
                cudaGraphExecDestroy(sc.graphExec);
                sc.graphExec = nullptr;
            }
            if (sc.graph) {
                cudaGraphDestroy(sc.graph);
                sc.graph = nullptr;
            }
            CUDA_CHECK(cudaGraphCreate(&sc.graph, 0));

            // --- Memset node: zero the cluster count at d_output[0:4] ---
            cudaMemsetParams ms_p = {};
            ms_p.dst = sc.d_output;
            ms_p.elementSize = 1;
            ms_p.width = sizeof(uint32_t);
            ms_p.height = 1;
            ms_p.pitch = sizeof(uint32_t);
            ms_p.value = 0;
            cudaGraphNode_t memset_node;
            CUDA_CHECK(cudaGraphAddMemsetNode(&memset_node, sc.graph, nullptr,
                                              0, &ms_p));

            // --- H2D node: host frame → d_frame (placeholder src) ---
            sc.h2d_params = {};
            sc.h2d_params.srcPtr = make_cudaPitchedPtr(
                m_placeholder, m_image_bytes, m_image_bytes, 1);
            sc.h2d_params.dstPtr = make_cudaPitchedPtr(
                sc.d_frame, m_image_bytes, m_image_bytes, 1);
            sc.h2d_params.extent = make_cudaExtent(m_image_bytes, 1, 1);
            sc.h2d_params.kind = cudaMemcpyHostToDevice;
            CUDA_CHECK(cudaGraphAddMemcpyNode(&sc.h2d_node, sc.graph, nullptr,
                                              0, &sc.h2d_params));

            // Kernel waits for both memset and H2D (d_frame and count zeroed)
            cudaGraphNode_t pre_kernel[] = {memset_node, sc.h2d_node};

            // --- Kernel node ---
            sc.karg_d_frame = sc.d_frame;
            sc.karg_d_pd_mean = sc.d_pd_mean;
            sc.karg_d_pd_sum = sc.d_pd_sum;
            sc.karg_d_pd_sum2 = sc.d_pd_sum2;
            sc.karg_d_pd_off = sc.d_pd_off;
            sc.karg_n_pd_samples = n_pd_samples;
            sc.karg_nSigma = m_nSigma;
            sc.karg_nrows = nrows;
            sc.karg_ncols = ncols;
            sc.karg_d_clusters = reinterpret_cast<ClusterType *>(
                sc.d_output + m_clusters_offset);
            sc.karg_d_cluster_count = reinterpret_cast<uint32_t *>(sc.d_output);
            sc.karg_max_clusters =
                static_cast<uint32_t>(m_max_clusters_per_frame);

            sc.kargs_ptrs[0] = &sc.karg_d_frame;
            sc.kargs_ptrs[1] = &sc.karg_d_pd_mean;
            sc.kargs_ptrs[2] = &sc.karg_d_pd_sum;
            sc.kargs_ptrs[3] = &sc.karg_d_pd_sum2;
            sc.kargs_ptrs[4] = &sc.karg_d_pd_off;
            sc.kargs_ptrs[5] = &sc.karg_n_pd_samples;
            sc.kargs_ptrs[6] = &sc.karg_nSigma;
            sc.kargs_ptrs[7] = &sc.karg_nrows;
            sc.kargs_ptrs[8] = &sc.karg_ncols;
            sc.kargs_ptrs[9] = &sc.karg_d_clusters;
            sc.kargs_ptrs[10] = &sc.karg_d_cluster_count;
            sc.kargs_ptrs[11] = &sc.karg_max_clusters;

            cudaKernelNodeParams kp = {};
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            kp.func = reinterpret_cast<void *>(
                device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE>);
            kp.gridDim = grid;
            kp.blockDim = block;
            kp.sharedMemBytes = static_cast<unsigned int>(shmem_bytes);
            kp.kernelParams = sc.kargs_ptrs;
            kp.extra = nullptr;

            cudaGraphNode_t kernel_node;
            CUDA_CHECK(cudaGraphAddKernelNode(&kernel_node, sc.graph,
                                              pre_kernel, 2, &kp));

            // --- D2H node: d_output → host slot (placeholder dst) ---
            sc.d2h_params = {};
            sc.d2h_params.srcPtr =
                make_cudaPitchedPtr(sc.d_output, m_output_bytes_per_frame,
                                    m_output_bytes_per_frame, 1);
            sc.d2h_params.dstPtr =
                make_cudaPitchedPtr(m_placeholder, m_output_bytes_per_frame,
                                    m_output_bytes_per_frame, 1);
            sc.d2h_params.extent =
                make_cudaExtent(m_output_bytes_per_frame, 1, 1);
            sc.d2h_params.kind = cudaMemcpyDeviceToHost;
            CUDA_CHECK(cudaGraphAddMemcpyNode(&sc.d2h_node, sc.graph,
                                              &kernel_node, 1, &sc.d2h_params));

            CUDA_CHECK(cudaGraphInstantiate(&sc.graphExec, sc.graph, nullptr,
                                            nullptr, 0));
        }
    }
};

} // namespace aare
