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
    // Device pedestal precision is set by DEVICE_PED_TYPE in the kernel header.
    // Accumulators hold CENTERED moments of Y = X - d_pd_off (see kernel):
    //   d_pd_sum  ~ n*E[Y], d_pd_sum2 ~ n*E[Y^2], d_pd_mean = full mean
    //   (X0+E[Y]).
    device::DEVICE_PED_TYPE *d_pd_mean = nullptr;
    device::DEVICE_PED_TYPE *d_pd_sum = nullptr;
    device::DEVICE_PED_TYPE *d_pd_sum2 = nullptr;
    device::DEVICE_PED_TYPE *d_pd_off = nullptr; // frozen per-pixel baseline X0
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
    // A BatchView handed out by collect_view() still points into this slot's
    // pinned buffer. The slot must not be reused until the view is released, so
    // submit_batch() refuses it rather than overwriting live results.
    bool m_slot_view_held[NUM_SLOTS] = {false, false};
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
    // Frozen per-pixel baseline X0 (~mean at t=0), captured once on the first
    // sync and reused so the centered device accumulators never need rebasing.
    // Cleared by clear_pedestal() to force re-capture on the next sync.
    std::vector<device::DEVICE_PED_TYPE> m_offset;

    using SC = StreamContext<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    std::vector<SC> v_sc;

    float m_total_kernel_ms = 0.0f;
    size_t m_frames_processed = 0;

    // Opt-in kernel timing. Off by default: it costs two cudaEventRecord per
    // frame (real stream operations) plus one cudaEventElapsedTime query per
    // frame on the host, and the number it produces is only meaningful when
    // kernels cannot queue behind one another — see avg_kernel_time_ms().
    bool m_time_kernels = false;

    // Frames per internally pipelined chunk in find_clusters_batched().
    // 0 = auto (batch split so one chunk is marshaled while the next runs).
    size_t m_batch_chunk = 0;

    // Per-slot kernel timing event pools (sized lazily to the largest batch).
    // Left empty when m_time_kernels is false — no events are ever created.
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

    /// Copy every frame of a pinned output slot into results, turning the
    /// packed device layout into owned ClusterVectors. This is exactly the work
    /// collect_view() exists to avoid.
    ///
    /// Single-threaded on purpose. Spreading this copy over a thread pool was
    /// tried and reverted: the work is one 467 kB malloc + first-touch per
    /// frame at 9x9, so it is allocation-bound rather than bandwidth-bound.
    /// Extra threads get their own glibc arenas, which destroys heap reuse
    /// between calls and costs more in page faults than the parallel copy saves
    /// — measured 2.27 M faults at 8 threads vs 9.7 k at 1, for a 6 % gain at
    /// best and a 33 % *loss* when results are freed promptly. The fix is to
    /// stop allocating per frame, not to copy faster.
    void
    materialize_slot(const void *slot_base, size_t n_frames,
                     std::vector<ClusterVector<ClusterType>> &results) const {
        for (size_t frame_idx = 0; frame_idx < n_frames; ++frame_idx) {
            const void *h_out = static_cast<const char *>(slot_base) +
                                frame_idx * m_output_bytes_per_frame;
            uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
            // The device counter increments past the cap (only the write is
            // guarded), so this clamp is an out-of-bounds guard, not a tuning
            // choice.
            n_found = std::min<uint32_t>(
                n_found, static_cast<uint32_t>(m_max_clusters_per_frame));

            if (n_found > 0) {
                const auto *src = reinterpret_cast<const ClusterType *>(
                    static_cast<const char *>(h_out) + m_clusters_offset);
                results[frame_idx].resize(n_found);
                std::memcpy(results[frame_idx].data(), src,
                            n_found * sizeof(ClusterType));
            }
        }
    }

    /// Default number of pipelined chunks per find_clusters_batched() call.
    /// Two competing costs set this:
    ///   - fill/drain: the first submit and last collect are not overlapped,
    ///     costing min(GPU, host) per batch = min/(C*max) of the total, so this
    ///     term FALLS as C grows;
    ///   - per-chunk tail: every chunk ends with all streams draining,
    ///     ~n_streams * kernel_time of partly-idle GPU, so this term RISES
    ///     linearly with C.
    /// Measured 9x9 rates (GPU 26.7 us/frame, host 58.8 us/frame, kernel
    /// 25.4 us, 2000 frames) put the sum at 23 % for C=2, 11 % for C=4,
    /// 6 % for C=8 and ~4 % for C=16-32 — a flat minimum past 16. C=8 sits
    /// inside the knee while keeping chunks large enough that each stream still
    /// gets a healthy number of frames. Override with set_batch_chunk() to
    /// measure rather than trust this.
    static constexpr size_t DEFAULT_BATCH_CHUNKS = 8;

    /// Upper bound on one pinned output slot. cudaMallocHost is page-locked, so
    /// its first touch is charged to whoever triggers it — keep it modest.
    static constexpr size_t MAX_SLOT_BYTES = 128ull << 20; // 128 MiB

    /// Frames per pipelined chunk in find_clusters_batched(). Rounded up to a
    /// multiple of n_streams so chunking never changes which stream a frame
    /// lands on (the device pedestal is per-stream, so a different assignment
    /// would mean a different pedestal state per frame — a correctness issue,
    /// not a tidiness one).
    size_t resolve_batch_chunk(size_t n_frames) const {
        const size_t ns = static_cast<size_t>(n_streams);
        size_t chunk = m_batch_chunk;
        if (chunk == 0) {
            chunk =
                (n_frames + DEFAULT_BATCH_CHUNKS - 1) / DEFAULT_BATCH_CHUNKS;
            // Keep at least a few frames per stream per chunk, or the per-chunk
            // drain dominates.
            const size_t min_chunk = ns * 4;
            if (chunk < min_chunk)
                chunk = min_chunk;
            // Cap by BYTES, not frames. Each slot is cudaMallocHost'd at
            // chunk * m_output_bytes_per_frame, and there are NUM_SLOTS of
            // them. Without this, find_clusters_batched(whole_array) scales the
            // pinned allocation with the array: 20 000 frames at 9x9 gives
            // 2500-frame chunks = 1.23 GB per slot, 2.46 GB pinned, whose
            // first-touch cost (~600 k page faults) lands inside the caller's
            // timed region. Bounding it makes one big call behave like a loop
            // over slices.
            const size_t max_by_bytes =
                std::max<size_t>(1, MAX_SLOT_BYTES / m_output_bytes_per_frame);
            if (chunk > max_by_bytes)
                chunk = std::max(max_by_bytes, ns);
        }
        chunk = ((chunk + ns - 1) / ns) * ns;
        return std::min(chunk, n_frames);
    }

    /// Called by BatchView when it is released, making the slot reusable.
    void release_slot(int slot) {
        if (slot >= 0 && slot < NUM_SLOTS)
            m_slot_view_held[slot] = false;
    }

    /// Wait for a slot's D2H to land and hand back its bookkeeping. Shared by
    /// collect() and collect_view() so the two cannot drift.
    void finish_slot(int slot, size_t &n_frames, uint64_t &first_frame) {
        if (!m_slot_in_flight[slot])
            throw std::runtime_error(
                "ClusterFinderCUDA: collect() called on a slot that is not "
                "in flight");
        n_frames = m_slot_n_frames[slot];
        first_frame = m_slot_first_frame[slot];
        // cudaEventSynchronize (not cudaStreamSynchronize) so a batch already
        // queued behind this one in the same streams is not waited on.
        for (int k = 0; k < m_slot_streams_used[slot]; ++k)
            CUDA_CHECK(cudaEventSynchronize(m_batch_done[slot][k]));
        accumulate_kernel_times(slot, n_frames);
        m_frames_processed += n_frames;
        m_slot_in_flight[slot] = false;
    }

    /// Accumulate per-frame kernel times for a slot. Serial by construction:
    /// it mutates m_total_kernel_ms and queries CUDA events.
    void accumulate_kernel_times(int slot, size_t n_frames) {
        if (!m_time_kernels)
            return;
        for (size_t frame_idx = 0; frame_idx < n_frames; ++frame_idx) {
            float kernel_ms = 0.0f;
            CUDA_CHECK(cudaEventElapsedTime(
                &kernel_ms, m_kernel_start_pools[slot][frame_idx],
                m_kernel_stop_pools[slot][frame_idx]));
            m_total_kernel_ms += kernel_ms;
        }
    }

  public:
    /**
     * @brief Opaque handle returned by submit_batch(). Pass to collect().
     */
    struct BatchToken {
        int slot;
    };

    using value_type = typename ClusterType::value_type;

    /**
     * @brief Zero-copy view of a batch, still in the pinned D2H buffer.
     *
     * The D2H staging buffer already holds correctly laid-out clusters at a
     * fixed per-frame stride, so there is nothing to copy: this just exposes
     * offsets into it. Host cost per frame drops to reading one counter.
     *
     * @warning The view borrows the finder's slot. It stays valid until
     * release() (or destruction); until then submit_batch() will refuse that
     * slot rather than overwrite live results. With NUM_SLOTS == 2 that means
     * you must finish with a view before submitting two more batches — the
     * intended pattern is consume-then-release, one chunk at a time.
     */
    class BatchView {
        friend class ClusterFinderCUDA;

        ClusterFinderCUDA *m_owner = nullptr;
        const uint8_t *m_base = nullptr;
        size_t m_n_frames = 0;
        uint64_t m_first_frame = 0;
        size_t m_stride = 0;    // bytes per frame in the pinned buffer
        size_t m_cl_offset = 0; // byte offset of the cluster array in a frame
        size_t m_max_clusters = 0;
        int m_slot = -1;

        BatchView(ClusterFinderCUDA *owner, const void *base, size_t n_frames,
                  uint64_t first_frame, size_t stride, size_t cl_offset,
                  size_t max_clusters, int slot)
            : m_owner(owner), m_base(static_cast<const uint8_t *>(base)),
              m_n_frames(n_frames), m_first_frame(first_frame),
              m_stride(stride), m_cl_offset(cl_offset),
              m_max_clusters(max_clusters), m_slot(slot) {}

      public:
        BatchView() = default;
        BatchView(const BatchView &) = delete;
        BatchView &operator=(const BatchView &) = delete;
        BatchView(BatchView &&o) noexcept { steal(o); }
        BatchView &operator=(BatchView &&o) noexcept {
            if (this != &o) {
                release();
                steal(o);
            }
            return *this;
        }
        ~BatchView() { release(); }

        /// Give the slot back. Idempotent; the view is unusable afterwards.
        void release() {
            if (m_owner)
                m_owner->release_slot(m_slot);
            m_owner = nullptr;
            m_base = nullptr;
            m_n_frames = 0;
            m_slot = -1;
        }

        bool valid() const { return m_base != nullptr; }
        size_t n_frames() const { return m_n_frames; }
        uint64_t first_frame() const { return m_first_frame; }

        uint32_t count(size_t i) const {
            check(i);
            uint32_t n =
                *reinterpret_cast<const uint32_t *>(m_base + i * m_stride);
            // The device counter increments past the cap (only the write is
            // guarded), so clamp: this is an out-of-bounds guard.
            return std::min<uint32_t>(n, static_cast<uint32_t>(m_max_clusters));
        }

        const ClusterType *clusters(size_t i) const {
            check(i);
            return reinterpret_cast<const ClusterType *>(m_base + i * m_stride +
                                                         m_cl_offset);
        }

        size_t total_clusters() const {
            size_t n = 0;
            for (size_t i = 0; i < m_n_frames; ++i)
                n += count(i);
            return n;
        }

        /// Per-cluster sums for the whole batch, concatenated frame by frame.
        /// The common reduction, done here so callers never have to materialise
        /// the clusters themselves.
        std::vector<value_type> sums() const {
            std::vector<value_type> out;
            out.reserve(total_clusters());
            for (size_t i = 0; i < m_n_frames; ++i) {
                const ClusterType *c = clusters(i);
                const uint32_t n = count(i);
                for (uint32_t j = 0; j < n; ++j)
                    out.push_back(c[j].sum());
            }
            return out;
        }

      private:
        void steal(BatchView &o) {
            m_owner = o.m_owner;
            m_base = o.m_base;
            m_n_frames = o.m_n_frames;
            m_first_frame = o.m_first_frame;
            m_stride = o.m_stride;
            m_cl_offset = o.m_cl_offset;
            m_max_clusters = o.m_max_clusters;
            m_slot = o.m_slot;
            o.m_owner = nullptr;
            o.m_base = nullptr;
            o.m_slot = -1;
        }
        void check(size_t i) const {
            if (!m_base)
                throw std::runtime_error(
                    "ClusterFinderCUDA::BatchView: view has been released");
            if (i >= m_n_frames)
                throw std::out_of_range(
                    "ClusterFinderCUDA::BatchView: frame index out of range");
        }
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
     * @param time_kernels              enable per-frame CUDA-event kernel
     * timing. Off by default: it adds two event records per frame to the
     * streams and one host-side query per frame, and the resulting number is
     * only meaningful at n_streams == 1 (see avg_kernel_time_ms()).
     */
    ClusterFinderCUDA(Shape<2> shape_, COMPUTE_TYPE nSigma = 5.0,
                      size_t max_clusters_per_frame = 2048, int n_streams_ = 4,
                      bool time_kernels = false)
        : m_shape(shape_), nrows(shape_[0]), ncols(shape_[1]),
          m_image_size(nrows * ncols), n_streams(n_streams_),
          m_max_clusters_per_frame(max_clusters_per_frame), m_nSigma(nSigma),
          m_pedestal(shape_[0], shape_[1]), m_clusters(max_clusters_per_frame),
          m_time_kernels(time_kernels) {
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
            if (sc.d_pd_off)
                cudaFree(sc.d_pd_off);
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
        m_offset.clear(); // re-capture the baseline on the next sync
        m_pedestal_dirty = true;
    }

    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }

    /**
     * @brief Device pedestal MEAN for one stream — the pedestal the kernel
     *        actually reads and updates in place every frame. This differs from
     *        pedestal() (the host pedestal, advanced only by
     *        push_pedestal_frame): it carries the in-kernel running update, so
     *        it is the baseline an accept/reject decision was really made
     *        against. Reading it right BEFORE a find_clusters() call gives the
     *        state that call will decide with (the kernel updates at frame
     * end). With the single-frame path every frame lands on stream 0.
     */
    NDArray<PEDESTAL_TYPE, 2> device_pedestal(int stream = 0) {
        if (m_pedestal_dirty) {
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }
        auto &sc = v_sc.at(static_cast<size_t>(stream));
        CUDA_CHECK(cudaStreamSynchronize(sc.stream));
        using DPT = device::DEVICE_PED_TYPE;
        std::vector<DPT> h_mean(m_image_size);
        CUDA_CHECK(cudaMemcpy(h_mean.data(), sc.d_pd_mean,
                              m_image_size * sizeof(DPT),
                              cudaMemcpyDeviceToHost));
        NDArray<PEDESTAL_TYPE, 2> out(
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)});
        for (size_t i = 0; i < m_image_size; ++i)
            out.data()[i] = static_cast<PEDESTAL_TYPE>(h_mean[i]);
        return out;
    }

    /**
     * @brief Device pedestal RMS for one stream, computed exactly as the kernel
     *        does: sqrt(max(sum2/n - mean^2, 0)). See device_pedestal().
     */
    NDArray<PEDESTAL_TYPE, 2> device_noise(int stream = 0) {
        if (m_pedestal_dirty) {
            sync_pedestal_to_device();
            m_pedestal_dirty = false;
        }
        auto &sc = v_sc.at(static_cast<size_t>(stream));
        CUDA_CHECK(cudaStreamSynchronize(sc.stream));
        using DPT = device::DEVICE_PED_TYPE;
        std::vector<DPT> h_mean(m_image_size), h_sum2(m_image_size);
        CUDA_CHECK(cudaMemcpy(h_mean.data(), sc.d_pd_mean,
                              m_image_size * sizeof(DPT),
                              cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_sum2.data(), sc.d_pd_sum2,
                              m_image_size * sizeof(DPT),
                              cudaMemcpyDeviceToHost));
        const double n = static_cast<double>(m_pedestal.n_samples());
        NDArray<PEDESTAL_TYPE, 2> out(
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)});
        for (size_t i = 0; i < m_image_size; ++i) {
            // Accumulators are centered on X0 = m_offset[i], so the variance is
            // E[Y^2] - E[Y]^2 with E[Y] = mean - X0 (mirrors the kernel).
            double resid = static_cast<double>(h_mean[i]) -
                           static_cast<double>(m_offset[i]);
            double var = static_cast<double>(h_sum2[i]) / n - resid * resid;
            out.data()[i] =
                static_cast<PEDESTAL_TYPE>(std::sqrt(std::max(var, 0.0)));
        }
        return out;
    }

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
        // Inert unless collect_view() was used: a live BatchView still points
        // into this slot's pinned buffer, so reusing it would overwrite results
        // the caller is still reading.
        if (m_slot_view_held[slot])
            throw std::runtime_error(
                "ClusterFinderCUDA: this batch slot is still held by a "
                "BatchView — release() it before submitting another batch");
        m_next_slot = 1 - slot;

        const size_t n_frames_batch = static_cast<size_t>(frames.shape(0));
        const uint32_t n_pd_samples =
            static_cast<uint32_t>(m_pedestal.n_samples());

        grow_output_slot(slot, n_frames_batch);
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
            if (m_time_kernels)
                CUDA_CHECK(cudaEventRecord(
                    m_kernel_start_pools[slot][frame_idx], sc.stream));
            device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE>
                <<<grid, block, shmem_bytes, sc.stream>>>(
                    sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
                    sc.d_pd_off, n_pd_samples, m_nSigma, nrows, ncols,
                    d_clusters, d_cluster_count,
                    static_cast<uint32_t>(m_max_clusters_per_frame));
            if (m_time_kernels)
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

        // for (size_t frame_idx = 0; frame_idx < n_frames_batch; ++frame_idx) {
        //     const void *h_out =
        //         static_cast<const char *>(h_output_slots[slot]) +
        //         frame_idx * m_output_bytes_per_frame;
        //     uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
        //     n_found = std::min<uint32_t>(
        //         n_found, static_cast<uint32_t>(m_max_clusters_per_frame));

        //     if (n_found > 0) {
        //         const auto *src = reinterpret_cast<const ClusterType *>(
        //             static_cast<const char *>(h_out) + m_clusters_offset);
        //         results[frame_idx].resize(n_found);
        //         std::memcpy(results[frame_idx].data(), src,
        //                     n_found * sizeof(ClusterType));
        //     }

        //     float kernel_ms = 0.0f;
        //     CUDA_CHECK(cudaEventElapsedTime(
        //         &kernel_ms, m_kernel_start_pools[slot][frame_idx],
        //         m_kernel_stop_pools[slot][frame_idx]));
        //     m_total_kernel_ms += kernel_ms;
        // }

        // materialize_slot() is the original copy loop verbatim, just hoisted
        // into a helper so find_clusters_batched() shares it;
        // accumulate_kernel_times is the m_time_kernels branch that used to
        // live in the same loop.
        materialize_slot(h_output_slots[slot], n_frames_batch, results);
        accumulate_kernel_times(slot, n_frames_batch);

        m_frames_processed += n_frames_batch;
        m_slot_in_flight[slot] = false;
        return results;
    }

    /**
     * @brief Collect a batch as a zero-copy view — no allocation, no copy.
     *
     * Nothing is copied and nothing is allocated: the returned view points into
     * the finder's pinned D2H buffer. The slot is held until the view is
     * released, so at most NUM_SLOTS - 1 further batches can be submitted while
     * it is alive. Consume, then release.
     */
    BatchView collect_view(BatchToken token) {
        const int slot = token.slot;
        size_t n_frames = 0;
        uint64_t first_frame = 0;
        finish_slot(slot, n_frames, first_frame);

        m_slot_view_held[slot] = true;
        return BatchView(this, h_output_slots[slot], n_frames, first_frame,
                         m_output_bytes_per_frame, m_clusters_offset,
                         m_max_clusters_per_frame, slot);
    }

    /**
     * @brief Synchronous batched cluster finding across multiple frames, using
     *        n_streams CUDA streams to overlap H2D, kernel, and D2H.
     *
     * Returns one ClusterVector per input frame (with frame_number set to
     * first_frame + i).
     *
     * Internally the batch is split into chunks and pipelined over the two
     * async slots: chunk i+1 is submitted before chunk i is collected, so the
     * host marshals one chunk while the GPU runs the next. A single call used
     * to be strictly `run the whole batch, then copy the whole batch`, leaving
     * the GPU idle for the entire host phase.
     *
     * Chunk size is rounded up to a multiple of n_streams so the frame->stream
     * assignment is identical to processing the batch in one go. That matters
     * for correctness, not just tidiness: the device pedestal is per-stream, so
     * changing which stream a frame lands on would change the pedestal state it
     * is evaluated against.
     *
     * @note Now shares the two batch slots with submit_batch()/collect(). A
     *       batch already in flight is no longer silently overwritten — you get
     *       an exception instead.
     */
    std::vector<ClusterVector<ClusterType>>
    find_clusters_batched(NDView<FRAME_TYPE, 3> frames,
                          uint64_t first_frame = 0) {
        const size_t n_frames_batch = static_cast<size_t>(frames.shape(0));
        if (n_frames_batch == 0)
            return {};

        const size_t chunk = resolve_batch_chunk(n_frames_batch);

        std::vector<ClusterVector<ClusterType>> results;
        results.reserve(n_frames_batch);

        auto drain = [&results](std::vector<ClusterVector<ClusterType>> part) {
            for (auto &cv : part)
                results.push_back(std::move(cv));
        };

        BatchToken tok = submit_batch(
            frames.sub_view(
                0, static_cast<ssize_t>(std::min(chunk, n_frames_batch))),
            first_frame);
        for (size_t b = chunk; b < n_frames_batch; b += chunk) {
            const size_t e = std::min(b + chunk, n_frames_batch);
            // Submit before collecting: the GPU starts chunk b while the host
            // is still copying chunk b - chunk out of the pinned slot.
            BatchToken nxt =
                submit_batch(frames.sub_view(static_cast<ssize_t>(b),
                                             static_cast<ssize_t>(e)),
                             first_frame + b);
            drain(collect(tok));
            tok = nxt;
        }
        drain(collect(tok));

        return results;
    }

    // Previous implementation: one launch loop over the whole batch, one
    // cudaStreamSynchronize per stream, then one single-threaded copy loop over
    // every frame. Kept for reference — it is what the numbers in
    // docs/ClusterFinderCUDA_benchmark_results.md opt3/opt4 (Act I, sections
    // 5-6) were measured against.
    //
    // std::vector<ClusterVector<ClusterType>>
    // find_clusters_batched(NDView<FRAME_TYPE, 3> frames,
    //                       uint64_t first_frame = 0) {
    //     if (m_pedestal_dirty) {
    //         sync_pedestal_to_device();
    //         m_pedestal_dirty = false;
    //     }
    //
    //     const size_t n_frames_batch =
    //         static_cast<size_t>(frames.shape(0));
    //     const uint32_t n_pd_samples =
    //         static_cast<uint32_t>(m_pedestal.n_samples());
    //
    //     // Lazy grow D2H output staging buffer (one slot per frame)
    //     if (n_frames_batch > m_output_slot_capacity[0]) {
    //         if (h_output_slots[0])
    //             CUDA_CHECK(cudaFreeHost(h_output_slots[0]));
    //         CUDA_CHECK(cudaMallocHost(&h_output_slots[0],
    //                                   n_frames_batch *
    //                                       m_output_bytes_per_frame));
    //         m_output_slot_capacity[0] = n_frames_batch;
    //     }
    //
    //     ensure_event_pool(0, n_frames_batch);
    //
    //     std::vector<ClusterVector<ClusterType>> results;
    //     results.reserve(n_frames_batch);
    //     for (size_t i = 0; i < n_frames_batch; ++i) {
    //         results.emplace_back();
    //         results.back().set_frame_number(first_frame + i);
    //     }
    //
    //     for (size_t frame_idx = 0; frame_idx < n_frames_batch;
    //          ++frame_idx) {
    //         auto &sc = v_sc[frame_idx % n_streams];
    //
    //         const FRAME_TYPE *h_src =
    //             frames.data() + frame_idx * m_image_size;
    //         auto *d_cluster_count =
    //             reinterpret_cast<uint32_t *>(sc.d_output);
    //
    //         CUDA_CHECK(cudaMemsetAsync(d_cluster_count, 0,
    //                                    sizeof(uint32_t), sc.stream));
    //         CUDA_CHECK(cudaMemcpyAsync(sc.d_frame, h_src, m_image_bytes,
    //                                    cudaMemcpyHostToDevice, sc.stream));
    //
    //         auto *d_clusters = reinterpret_cast<ClusterType *>(
    //             sc.d_output + m_clusters_offset);
    //         if (m_time_kernels)
    //             CUDA_CHECK(cudaEventRecord(
    //                 m_kernel_start_pools[0][frame_idx], sc.stream));
    //         device::find_clusters_in_single_frame<ClusterType, FRAME_TYPE>
    //             <<<grid, block, shmem_bytes, sc.stream>>>(
    //                 sc.d_frame, sc.d_pd_mean, sc.d_pd_sum, sc.d_pd_sum2,
    //                 sc.d_pd_off, n_pd_samples, m_nSigma, nrows, ncols,
    //                 d_clusters, d_cluster_count,
    //                 static_cast<uint32_t>(m_max_clusters_per_frame));
    //         if (m_time_kernels)
    //             CUDA_CHECK(cudaEventRecord(
    //                 m_kernel_stop_pools[0][frame_idx], sc.stream));
    //         CUDA_CHECK(cudaGetLastError());
    //
    //         void *h_out = static_cast<char *>(h_output_slots[0]) +
    //                       frame_idx * m_output_bytes_per_frame;
    //         CUDA_CHECK(cudaMemcpyAsync(h_out, sc.d_output,
    //                                    m_output_bytes_per_frame,
    //                                    cudaMemcpyDeviceToHost, sc.stream));
    //     }
    //
    //     const int streams_used =
    //         std::min<int>(n_streams, static_cast<int>(n_frames_batch));
    //     for (int k = 0; k < streams_used; ++k)
    //         CUDA_CHECK(cudaStreamSynchronize(v_sc[k].stream));
    //
    //     for (size_t frame_idx = 0; frame_idx < n_frames_batch;
    //          ++frame_idx) {
    //         const void *h_out =
    //             static_cast<const char *>(h_output_slots[0]) +
    //             frame_idx * m_output_bytes_per_frame;
    //         uint32_t n_found = *reinterpret_cast<const uint32_t *>(h_out);
    //         n_found = std::min<uint32_t>(
    //             n_found, static_cast<uint32_t>(m_max_clusters_per_frame));
    //
    //         if (n_found > 0) {
    //             const auto *src = reinterpret_cast<const ClusterType *>(
    //                 static_cast<const char *>(h_out) + m_clusters_offset);
    //             results[frame_idx].resize(n_found);
    //             std::memcpy(results[frame_idx].data(), src,
    //                         n_found * sizeof(ClusterType));
    //         }
    //
    //         if (m_time_kernels) {
    //             float kernel_ms = 0.0f;
    //             CUDA_CHECK(cudaEventElapsedTime(
    //                 &kernel_ms, m_kernel_start_pools[0][frame_idx],
    //                 m_kernel_stop_pools[0][frame_idx]));
    //             m_total_kernel_ms += kernel_ms;
    //         }
    //     }
    //
    //     m_frames_processed += n_frames_batch;
    //     return results;
    // }

    /// True if per-frame kernel timing was enabled at construction.
    bool kernel_timing_enabled() const { return m_time_kernels; }

    /**
     * @brief Frames per internally pipelined chunk in find_clusters_batched().
     *
     * 0 (default) = auto: the batch is split into ~8 chunks so the host can
     * marshal one chunk while the GPU runs the next. Rounded up to a multiple
     * of n_streams. Set equal to the batch size to disable chunking and get
     * the old submit-everything-then-copy-everything behaviour.
     */
    void set_batch_chunk(size_t n) { m_batch_chunk = n; }
    size_t get_batch_chunk() const { return m_batch_chunk; }

    /// The chunk size find_clusters_batched() would use for n_frames. Exposed
    /// so a caller driving submit/collect_view by hand can match its pipelining
    /// without duplicating the rounding rules.
    size_t chunk_size_for(size_t n_frames) const {
        return resolve_batch_chunk(n_frames);
    }

    /**
     * @brief Pre-allocate both pinned output slots for batches of n_frames.
     *
     * Processes nothing: no frame is transferred, no kernel is launched, and
     * the pedestal is untouched. Only the two cudaMallocHost calls that
     * submit_batch() would otherwise make on its first invocation happen here.
     *
     * The point is that page-locking is expensive and is charged to whoever
     * triggers it: measured ~1.0 us per 4 kB page, i.e. ~66 ms for two 128 MiB
     * slots (and ~40 % more if an undersized slot has to be freed first). Left
     * to submit_batch() that cost lands inside the first timed region — worth
     * 2.8 us/frame over 20 000 frames at 3x3, which is 17 % of the 16.2 us
     * roofline. Call this before starting a timer, or before a
     * latency-sensitive first batch. Slots only ever grow, so a later smaller
     * batch is free and a larger one still re-allocates: pass the largest batch
     * you intend to use, e.g. chunk_size_for(n) when driving
     * find_clusters_batched() or the submit/collect_view loop over n frames.
     */
    void reserve_output_slots(size_t n_frames) {
        for (int slot = 0; slot < NUM_SLOTS; ++slot)
            grow_output_slot(slot, n_frames);
    }

    /**
     * @brief Average per-frame kernel time in ms, or NaN if timing is disabled.
     *
     * @warning Only meaningful at n_streams == 1. The CUDA events bracket the
     * kernel on its own stream, so under multi-stream contention the measured
     * interval includes time queued behind kernels from other streams — it
     * over-reads by up to ~3.5x. Use Nsight Systems for exclusive kernel times.
     */
    float avg_kernel_time_ms() const {
        if (!m_time_kernels)
            return std::numeric_limits<float>::quiet_NaN();
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

        using DPT = device::DEVICE_PED_TYPE;
        const double n = static_cast<double>(m_pedestal.n_samples());

        // Capture the frozen per-pixel baseline X0 (≈ mean at t=0) ONCE. Fixed
        // for the run so the centered device accumulators never need rebasing;
        // clear_pedestal() empties m_offset to force a fresh capture.
        if (m_offset.size() != m_image_size) {
            m_offset.resize(m_image_size);
            for (size_t i = 0; i < m_image_size; ++i)
                m_offset[i] = static_cast<DPT>(std::llround(h_mean.data()[i]));
        }

        // Center in DOUBLE (where sum2 ~ n·E[X²] is still exact), then cast the
        // SMALL centered results to DPT — this is what dodges the f32
        // cancellation. Device holds: sum ~ n·E[Y], sum2 ~ n·E[Y²], Y = X − X0.
        //   Σ(X−X0)   = ΣX  − n·X0
        //   Σ(X−X0)²  = ΣX² − 2·X0·ΣX + n·X0²
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
    }

    /// Grow one slot's pinned D2H buffer to hold n_frames. Only ever grows, so
    /// a run whose batches keep the same shape allocates once.
    void grow_output_slot(int slot, size_t n_frames) {
        if (n_frames <= m_output_slot_capacity[slot])
            return;
        if (h_output_slots[slot])
            CUDA_CHECK(cudaFreeHost(h_output_slots[slot]));
        CUDA_CHECK(cudaMallocHost(&h_output_slots[slot],
                                  n_frames * m_output_bytes_per_frame));
        m_output_slot_capacity[slot] = n_frames;
    }

    void ensure_event_pool(int slot, size_t n_frames) {
        if (!m_time_kernels)
            return; // no events are created when timing is disabled
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
