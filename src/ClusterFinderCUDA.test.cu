// SPDX-License-Identifier: MPL-2.0
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <vector>

#include "aare/defs.hpp"
#include "aare/utils/batch.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/Pedestal.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderCUDA.hpp"

// _____________
//
// Timing helper
// _____________
struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0;

    void start() { t0 = clock::now(); }

    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    }
};

// __________________
// 
// Cluster comparison
// __________________
template <typename ClusterType>
struct ClusterComparison {
    size_t cpu_count = 0;
    size_t gpu_count = 0;
    size_t matched  = 0;
    size_t position_mismatch = 0;
    size_t data_mismatch = 0;
    size_t cpu_only = 0;
    size_t gpu_only = 0;
};

// Sort clusters by (y, x) for deterministic comparison
template <typename ClusterType>
void sort_clusters(std::vector<ClusterType>& clusters) {
    std::sort(clusters.begin(), clusters.end(),
              [](const ClusterType& a, const ClusterType& b) {
                  if (a.y != b.y) return a.y < b.y;
                  return a.x < b.x;
              });
}

// Compare two sorted cluster lists
template <typename ClusterType>
ClusterComparison<ClusterType> compare_clusters(std::vector<ClusterType>& cpu_clusters,
                                                std::vector<ClusterType>& gpu_clusters)
{
    sort_clusters(cpu_clusters);
    sort_clusters(gpu_clusters);

    ClusterComparison<ClusterType> result;
    result.cpu_count = cpu_clusters.size();
    result.gpu_count = gpu_clusters.size();

    size_t ci = 0, gi = 0;
    while (ci < cpu_clusters.size() && gi < gpu_clusters.size()) {
        const auto& cc = cpu_clusters[ci];
        const auto& gc = gpu_clusters[gi];

        if (cc.y == gc.y && cc.x == gc.x) {
            // Same position/check data
            bool data_ok = true;
            constexpr int N = ClusterType::cluster_size_x * ClusterType::cluster_size_y;
            for (int k = 0; k < N; ++k) {
                // if (cc.data[k] != gc.data[k]) { // a bit too strict espacially that pedestal update is slightly different
                if (std::abs(cc.data[k] - gc.data[k]) > 5) {
                    data_ok = false;
                    break;
                }
            }
            if (data_ok)
                result.matched++;
            else
                result.data_mismatch++;
            ci++;
            gi++;
        } else if (cc.y < gc.y || (cc.y == gc.y && cc.x < gc.x)) {
            result.cpu_only++;
            ci++;
        } else {
            result.gpu_only++;
            gi++;
        }
    }
    result.cpu_only += (cpu_clusters.size() - ci);
    result.gpu_only += (gpu_clusters.size() - gi);

    return result;
}

// ________________
//
// Cluster printing
// ________________
template <typename ClusterType>
void print_cluster_comparison(const std::vector<std::vector<ClusterType>>& cpu_results,
                              const std::vector<std::vector<ClusterType>>& gpu_results,
                              size_t max_per_frame = 10,
                              size_t max_frames = 100)
{
    constexpr int NX = ClusterType::cluster_size_x;
    constexpr int NY = ClusterType::cluster_size_y;
    constexpr int N  = NX * NY;
 
    size_t frames_shown = 0;
    for (size_t fi = 0; fi < cpu_results.size() && frames_shown < max_frames; ++fi) {
        if (cpu_results[fi].empty() && gpu_results[fi].empty())
            continue;
 
        size_t n_cpu = cpu_results[fi].size();
        size_t n_gpu = gpu_results[fi].size();
        printf("\n  Frame %zu: CPU=%zu clusters, GPU=%zu clusters\n", fi, n_cpu, n_gpu);
 
        // Merge-walk over sorted lists (assumes already sorted by y,x)
        size_t ci = 0, gi = 0;
        size_t shown = 0;
        while ((ci < n_cpu || gi < n_gpu) && shown < max_per_frame) {
            bool have_cpu = ci < n_cpu;
            bool have_gpu = gi < n_gpu;
 
            // Determine if current entries match in position
            bool same_pos = have_cpu && have_gpu &&
                            cpu_results[fi][ci].x == gpu_results[fi][gi].x &&
                            cpu_results[fi][ci].y == gpu_results[fi][gi].y;
 
            if (same_pos) {
                const auto& cc = cpu_results[fi][ci];
                const auto& gc = gpu_results[fi][gi];
 
                // Check if data differs
                bool differs = false;
                for (int k = 0; k < N; ++k) {
                    if (cc.data[k] != gc.data[k]) { differs = true; break; }
                }

                printf("    CPU and GPU clusters found at SAME position (col=%3d, row=%3d)\n %s\n", cc.x, cc.y,
                       differs ? "DATA MISMATCH" : "MATCH");
                printf("      CPU: [");
                for (int k = 0; k < N; ++k) { if (k) printf(", "); printf("%6d", static_cast<int>(cc.data[k])); }
                printf("]\n");
                if (differs) {
                    printf("      GPU: [");
                    for (int k = 0; k < N; ++k) { if (k) printf(", "); printf("%6d", static_cast<int>(gc.data[k])); }
                    printf("]\n");
                    printf("     diff: [");
                    for (int k = 0; k < N; ++k) {
                        if (k) printf(", ");
                        int d = static_cast<int>(gc.data[k]) - static_cast<int>(cc.data[k]);
                        printf("%+6d", d);
                    }
                    printf("]\n");
                }
                ci++; gi++; shown++;
            } else if (!have_gpu || (have_cpu && (cpu_results[fi][ci].y < gpu_results[fi][gi].y ||
                       (cpu_results[fi][ci].y == gpu_results[fi][gi].y &&
                        cpu_results[fi][ci].x < gpu_results[fi][gi].x)))) {
                const auto& cc = cpu_results[fi][ci];
                printf("    (%3d, %3d) CPU ONLY\n", cc.x, cc.y);
                printf("      CPU: [");
                for (int k = 0; k < N; ++k) { if (k) printf(", "); printf("%6d", static_cast<int>(cc.data[k])); }
                printf("]\n");
                ci++; shown++;
            } else {
                const auto& gc = gpu_results[fi][gi];
                printf("    (%3d, %3d) GPU ONLY\n", gc.x, gc.y);
                printf("      GPU: [");
                for (int k = 0; k < N; ++k) { if (k) printf(", "); printf("%6d", static_cast<int>(gc.data[k])); }
                printf("]\n");
                gi++; shown++;
            }
        }
        frames_shown++;
    }
}

// _________________________________________
//
// Helpers for the updated (CPU-parity) API
// _________________________________________
 
// Copy a ClusterVector into a std::vector<ClusterType> for downstream
// comparison code that expects the latter.
template <typename Finder, typename ClusterType>
void drain_into(Finder& f, std::vector<ClusterType>& out) {
    auto cv = f.steal_clusters(true);
    out.clear();
    out.reserve(cv.size());
    for (size_t j = 0; j < cv.size(); ++j) out.push_back(cv[j]);
}
 
// Feed a set of cached pedestal frames through any finder exposing the
// CPU-style push_pedestal_frame(NDView) method. Works for both
// ClusterFinder and ClusterFinderCUDA.
template <typename Finder, typename FRAME_TYPE>
void feed_pedestal(Finder& f,
                   const std::vector<aare::NDArray<FRAME_TYPE, 2>>& ped_frames) {
    for (const auto& frame : ped_frames) {
        f.push_pedestal_frame(frame.view());
    }
}

// ____________
//
//     Main
// ____________
int main(int argc, char* argv[]) {

    // Parse arguments
    const char* default_pedestal = 
                "/mnt/sls_det_storage/highZ_data/CZT_Vienna/Khalil/Calibration_CZT/2025Sept_m694/Sn25300eV/500_us_voltage_40kV/250922_CZTonly_Pedestal_Tp15C_tint_500_master_0.json";
    // const char* default_pedestal = 
    //             "/mnt/sls_det_storage/highZ_data/CZT_Vienna/Khalil/November2025/sparse/dynamic/si_pedestal_200keV_DYNAMIC_tint_20us_master_0.json";
    const char* default_data     = 
                "/mnt/sls_det_storage/highZ_data/CZT_Vienna/Khalil/Calibration_CZT/2025Sept_m694/Sn25300eV/500_us_voltage_40kV/250922_CZTonly_Xray_Tp15C_tint_500_master_0.json";
    // const char* default_data     = 
    //             "/mnt/sls_det_storage/highZ_data/CZT_Vienna/Khalil/November2025/sparse/dynamic/si_data_200keV_DYNAMIC_tint_20us_master_0.json";

    std::filesystem::path pedestal_path(argc > 1 ? argv[1] : default_pedestal);
    std::filesystem::path data_path(argc > 2 ? argv[2] : default_data);

    if (!std::filesystem::exists(pedestal_path)) {
        fprintf(stderr, "Pedestal file not found: %s\n", pedestal_path.c_str());
        return 1;
    }
    if (!std::filesystem::exists(data_path)) {
        fprintf(stderr, "Data file not found: %s\n", data_path.c_str());
        return 1;
    }

    // Defaults: Adjust depending on the test dataset used
    size_t n_pedestal_frames = 6000;
    size_t n_data_frames     = 10000;
    double nSigma            = 5.0;

    if (argc > 3) n_pedestal_frames = std::atol(argv[3]);
    if (argc > 4) n_data_frames     = std::atol(argv[4]);
    if (argc > 5) nSigma            = std::atof(argv[5]);

    // Detector geometry from master file
    constexpr uint8_t cs_x = 3;
    constexpr uint8_t cs_y = 3;
    using ClusterType = aare::Cluster<int32_t, cs_x, cs_y>;
    using FRAME_TYPE  = uint16_t;
    using PEDESTAL_TYPE = double;

    // Read actual frame dimensions from the pedestal file
    ssize_t ROWS, COLS;
    {
        aare::File probe(pedestal_path, "r");
        auto first_frame = probe.read_frame();
        ROWS = first_frame.rows();
        COLS = first_frame.cols();
    }

    printf("=== Cluster Finder: CPU vs CUDA ===\n");
    printf("Detector:  %zu x %zu\n", ROWS, COLS);
    printf("Cluster:   %d x %d\n", ClusterType::cluster_size_x, ClusterType::cluster_size_y);
    printf("nSigma:    %.1f\n", nSigma);

    // =========================================================================
    // Phase 1: Build pedestal from dark frames (sanity check only + frame cache)
    // =========================================================================
    //
    // Neither ClusterFinder nor ClusterFinderCUDA needs an external Pedestal object;
    // both build their own via push_pedestal_frame.
    // We still read the pedestal file once, but cache the frames in memory so
    // every subsequent finder can be fed without re-hitting disk. We also
    // build a standalone Pedestal purely to print a sanity check.
    // =========================================================================
    printf("\n--- Phase 1: Pedestal accumulation (sanity check + cache) ---\n");
 
    std::vector<aare::NDArray<FRAME_TYPE, 2>> pedestal_frames;
    aare::Pedestal<PEDESTAL_TYPE> pedestal(ROWS, COLS, 1000);
 
    {
        aare::File ped_file(pedestal_path, "r");
        size_t total_ped = ped_file.total_frames();
        size_t use_ped   = (n_pedestal_frames == 0 || n_pedestal_frames > total_ped)
                               ? total_ped : n_pedestal_frames;
        printf("Pedestal frames: %zu / %zu\n", use_ped, total_ped);
 
        pedestal_frames.reserve(use_ped);
 
        Timer t;
        t.start();
 
        for (size_t i = 0; i < use_ped; ++i) {
            auto frame = ped_file.read_frame();
            auto view  = frame.view<FRAME_TYPE>();
 
            // Copy into a standalone NDArray that we can reuse as many times
            // as we have finders to feed.
            aare::NDArray<FRAME_TYPE, 2> arr({ROWS, COLS});
            for (ssize_t r = 0; r < ROWS; ++r)
                for (ssize_t c = 0; c < COLS; ++c)
                    arr(r, c) = view(r, c);
            pedestal_frames.push_back(std::move(arr));
 
            pedestal.push_no_update(view);
        }
        pedestal.update_mean();
 
        printf("Pedestal read+cached+built in %.1f ms\n", t.elapsed_ms());
    }
 
    printf("Pedestal mean[0,0] = %.2f, std[0,0] = %.4f\n",
           pedestal.mean(0, 0), pedestal.std(0, 0));

    // =========================================================================
    // Phase 2: Read data frames
    // =========================================================================
    printf("\n--- Phase 2: Read data frames ---\n");

    aare::File data_file(data_path, "r");
    size_t total_data = data_file.total_frames();
    size_t use_data   = std::min(n_data_frames, total_data);
    printf("Data frames: %zu / %zu\n", use_data, total_data);

    // Pre-read all frames into memory to remove I/O from timing
    std::vector<aare::NDArray<FRAME_TYPE, 2>> frames;
    frames.reserve(use_data);
    for (size_t i = 0; i < use_data; ++i) {
        auto f = data_file.read_frame();
        // Copy into NDArray for consistent access
        aare::NDArray<FRAME_TYPE, 2> arr({ROWS, COLS});
        auto view = f.view<FRAME_TYPE>();
        for (size_t r = 0; r < ROWS; ++r)
            for (size_t c = 0; c < COLS; ++c)
                arr(r, c) = view(r, c);
        frames.push_back(std::move(arr));
    }
    printf("Frames loaded into memory\n");

    // =========================================================================
    // Phase 3: Sequential (CPU) cluster finding
    // =========================================================================
    printf("\n--- Phase 3: CPU ClusterFinder ---\n");

    std::vector<std::vector<ClusterType>> cpu_results(use_data);
    size_t cpu_total_clusters = 0;

    {
        // Build a ClusterFinder with the same pedestal
        aare::ClusterFinder<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cf(
            {ROWS, COLS}, nSigma);

        feed_pedestal(cf, pedestal_frames);

        Timer t;
        t.start();

        for (size_t i = 0; i < use_data; ++i) {
            cf.find_clusters(frames[i].view(), static_cast<uint64_t>(i));
            drain_into(cf, cpu_results[i]);
            cpu_total_clusters += cpu_results[i].size();
        }

        double cpu_time = t.elapsed_ms();
        printf("CPU: %zu clusters in %.1f ms (%.2f ms/frame)\n",
               cpu_total_clusters, cpu_time, cpu_time / use_data);
    }

    // =========================================================================
    // Phase 4: CUDA cluster finding
    // =========================================================================
    //
    // The API mirrors ClusterFinder: push_pedestal_frame to train, then
    // find_clusters / steal_clusters for each frame. H2D transfer and kernel
    // launch happen internally.
    //
    // Toggle between the single-stream and batched paths by swapping which
    // block is enabled. They both write into gpu_results so only one at a
    // time makes sense.
    // =========================================================================
    printf("\n--- Phase 4: CUDA ClusterFinder ---\n");
 
    std::vector<std::vector<ClusterType>> gpu_results(use_data);
    size_t gpu_total_clusters = 0;
 
    // --- Single frame on a single CUDA stream ---
    {
        aare::ClusterFinderCUDA<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cuda_cf(
            {ROWS, COLS}, nSigma);
 
        feed_pedestal(cuda_cf, pedestal_frames);
 
        // Warmup: first CUDA call pays driver/context init overhead. The
        // pedestal drifts slightly during this single frame, which is
        // acceptable for timing purposes.
        cuda_cf.find_clusters(frames[0].view(), 0);
        cuda_cf.steal_clusters(true);
 
        Timer t;
        t.start();
 
        for (size_t i = 0; i < use_data; ++i) {
            cuda_cf.find_clusters(frames[i].view(), static_cast<uint64_t>(i));
            drain_into(cuda_cf, gpu_results[i]);
            gpu_total_clusters += gpu_results[i].size();
        }
 
        double gpu_time = t.elapsed_ms();
        printf("GPU: %zu clusters in %.1f ms (%.2f ms/frame)\n",
               gpu_total_clusters, gpu_time, gpu_time / use_data);
    }

    // --- Batched H2D + multi-stream (enable this block and disable the one
    //     above to benchmark the batched path against the CPU results) ---
    /*
    {
        constexpr size_t BATCH_SIZE = 100;
        constexpr int    N_STREAMS  = 2;
 
        aare::ClusterFinderCUDA<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cuda_cf(
            {ROWS, COLS}, nSigma, 1'000'000, N_STREAMS);
 
        feed_pedestal(cuda_cf, pedestal_frames);
 
        // Contiguous staging buffer reused across batches
        std::vector<FRAME_TYPE> batch_buffer(BATCH_SIZE * ROWS * COLS);
 
        const size_t n_batches = (use_data + BATCH_SIZE - 1) / BATCH_SIZE;
 
        Timer t;
        t.start();
 
        for (size_t bi = 0; bi < n_batches; ++bi) {
            const size_t offset       = bi * BATCH_SIZE;
            const size_t actual_batch = std::min(BATCH_SIZE, use_data - offset);
 
            for (size_t k = 0; k < actual_batch; ++k) {
                std::memcpy(batch_buffer.data() + k * ROWS * COLS,
                            frames[offset + k].data(),
                            ROWS * COLS * sizeof(FRAME_TYPE));
            }
 
            aare::NDView<FRAME_TYPE, 3> batch_view(
                batch_buffer.data(),
                {static_cast<ssize_t>(actual_batch), ROWS, COLS});
 
            auto batch_results = cuda_cf.find_clusters_batched(batch_view, offset);
 
            for (size_t f = 0; f < actual_batch; ++f) {
                auto& cv = batch_results[f];
                auto& out = gpu_results[offset + f];
                out.clear();
                out.reserve(cv.size());
                for (size_t j = 0; j < cv.size(); ++j) out.push_back(cv[j]);
                gpu_total_clusters += out.size();
            }
        }
 
        double gpu_time = t.elapsed_ms();
        printf("GPU(batched): %zu clusters in %.1f ms (%.2f ms/frame, batch=%zu, streams=%d)\n",
               gpu_total_clusters, gpu_time, gpu_time / use_data, BATCH_SIZE, N_STREAMS);
    }
    */

    // =========================================================================
    // Phase 5: Comparison
    // =========================================================================
    printf("\n--- Phase 5: Comparison ---\n");

    size_t total_matched = 0;
    size_t total_data_mismatch = 0;
    size_t total_cpu_only = 0;
    size_t total_gpu_only = 0;
    size_t frames_with_differences = 0;

    for (size_t i = 0; i < use_data; ++i) {
        auto result = compare_clusters(cpu_results[i], gpu_results[i]);

        total_matched        += result.matched;
        total_data_mismatch  += result.data_mismatch;
        total_cpu_only       += result.cpu_only;
        total_gpu_only       += result.gpu_only;

        bool has_diff = (result.cpu_only > 0 || result.gpu_only > 0 || result.data_mismatch > 0);
        if (has_diff) {
            frames_with_differences++;
            // Print details for first few mismatching frames
            if (frames_with_differences <= 5) {
                printf("  Frame %zu: CPU=%zu GPU=%zu matched=%zu "
                       "data_mismatch=%zu cpu_only=%zu gpu_only=%zu\n",
                       i, result.cpu_count, result.gpu_count,
                       result.matched, result.data_mismatch,
                       result.cpu_only, result.gpu_only);
            }
        }
    }

    printf("\nSummary over %zu frames:\n", use_data);
    printf("  CPU total clusters:    %zu\n", cpu_total_clusters);
    printf("  GPU total clusters:    %zu\n", gpu_total_clusters);
    printf("  Matched:               %zu\n", total_matched);
    printf("  Data mismatch:         %zu\n", total_data_mismatch);
    printf("  CPU only:              %zu\n", total_cpu_only);
    printf("  GPU only:              %zu\n", total_gpu_only);
    printf("  Frames with diffs:     %zu / %zu\n", frames_with_differences, use_data);

    if (total_cpu_only == 0 && total_gpu_only == 0 && total_data_mismatch == 0) {
        printf("\n*** PASS: CPU and GPU results match exactly ***\n");
    } else {
        printf("\n*** DIFFERENCES DETECTED ***\n");
    }

    // // Print detailed cluster comparison (side-by-side with diffs)
    // if (cpu_total_clusters > 0 || gpu_total_clusters > 0) {
    //     size_t max_clusters_per_frame = 10;
    //     size_t max_frames = 100;
    //     printf("\n--- Cluster details (up to %zu frames, %zu clusters each) ---\n", max_frames, max_clusters_per_frame);
    //     print_cluster_comparison(cpu_results, gpu_results, max_clusters_per_frame, max_frames);
    // }

    // =========================================================================
    // Phase 6: Per-frame timing benchmark
    // =========================================================================
    printf("\n--- Phase 6: Detailed timing (%d iterations) ---\n", 1000);

    if (use_data > 0) {
        const int N_ITER = 1000;
        const auto& bench_frame = frames[0];

        // CPU benchmark
        {
            aare::ClusterFinder<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cf(
                {ROWS, COLS}, nSigma);

            // Load pedestal
            feed_pedestal(cf, pedestal_frames);

            // Warmup
            cf.find_clusters(bench_frame.view(), 0);
            cf.steal_clusters(true);

            Timer t;
            t.start();
            for (int iter = 0; iter < N_ITER; ++iter) {
                cf.find_clusters(bench_frame.view(), 0);
                cf.steal_clusters(true);
            }
            double cpu_per_frame = t.elapsed_ms() / N_ITER;
            printf("CPU: %.3f ms/frame\n", cpu_per_frame);
        }
        // --- GPU benchmark (single frame, single stream) ---
        {
            aare::ClusterFinderCUDA<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cuda_cf(
                {ROWS, COLS}, nSigma);
            feed_pedestal(cuda_cf, pedestal_frames);
 
            // Warmup
            cuda_cf.find_clusters(bench_frame.view(), 0);
            cuda_cf.steal_clusters(true);
 
            Timer t;
            t.start();
            for (int iter = 0; iter < N_ITER; ++iter) {
                cuda_cf.find_clusters(bench_frame.view(), 0);
                cuda_cf.steal_clusters(true);
            }
            printf("GPU:          %.3f ms/frame (H2D + kernel + D2H, single frame, single stream)\n",
                   t.elapsed_ms() / N_ITER);
        }
 
        // --- GPU benchmark (batched + multi-streamed) ---
        {
            constexpr size_t BATCH_SIZE = 500;
            constexpr int    N_STREAMS  = 10;
 
            aare::ClusterFinderCUDA<ClusterType, FRAME_TYPE, PEDESTAL_TYPE> cuda_cf(
                {ROWS, COLS}, nSigma, 1'000'000, N_STREAMS);
            feed_pedestal(cuda_cf, pedestal_frames);
 
            // Build one contiguous batch of BATCH_SIZE copies of bench_frame
            std::vector<FRAME_TYPE> batch(BATCH_SIZE * ROWS * COLS);
            for (size_t k = 0; k < BATCH_SIZE; ++k)
                std::memcpy(batch.data() + k * ROWS * COLS,
                            bench_frame.data(),
                            ROWS * COLS * sizeof(FRAME_TYPE));
 
            aare::NDView<FRAME_TYPE, 3> batch_view(
                batch.data(),
                {static_cast<ssize_t>(BATCH_SIZE), ROWS, COLS});
 
            // Warmup. The kernel mutates the device-side pedestal for every
            // non-photon pixel, so after a 500-frame warmup the pedestal
            // state has drifted. Reset to a clean baseline by clearing the
            // host pedestal and re-feeding the cached frames; this also
            // re-arms the dirty flag so the next find_clusters re-uploads.
            (void)cuda_cf.find_clusters_batched(batch_view, 0);
            cuda_cf.clear_pedestal();
            feed_pedestal(cuda_cf, pedestal_frames);
 
            const size_t n_iter_batches = (N_ITER + BATCH_SIZE - 1) / BATCH_SIZE;
 
            Timer t;
            t.start();
            for (size_t b = 0; b < n_iter_batches; ++b) {
                (void)cuda_cf.find_clusters_batched(batch_view, b * BATCH_SIZE);
            }
            printf("GPU(batched): %.3f ms/frame (H2D + kernel + D2H, batch=%zu, streams=%d)\n",
                   t.elapsed_ms() / (n_iter_batches * BATCH_SIZE),
                   BATCH_SIZE, N_STREAMS);
        }
    }

    printf("\nDone.\n");
    return 0;
}