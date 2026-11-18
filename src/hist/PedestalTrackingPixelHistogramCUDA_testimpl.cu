// SPDX-License-Identifier: MPL-2.0
#include "PedestalTrackingPixelHistogramCUDA_testimpl.hpp"
#include "aare/hist/PedestalTrackingPixelHistogramCUDA.hpp"
#include <optional>
#include <stdexcept>

namespace aare::test_cuda {

namespace {

using Hist = cuda::PedestalTrackingPixelHistogramCUDA<uint16_t>;

NDView<uint16_t, 2> frame_of(NDView<uint16_t, 3> frames, ssize_t i) {
    return NDView<uint16_t, 2>(frames.data() +
                                   i * frames.shape(1) * frames.shape(2),
                               {frames.shape(1), frames.shape(2)});
}

template <typename Exception, typename F> bool throws(F &&f) {
    try {
        f();
    } catch (const Exception &) {
        return true;
    }
    return false;
}

} // namespace

std::vector<TrackingSnapshot>
run_tracking_gpu(NDView<uint16_t, 3> before, NDView<uint16_t, 3> seed,
                 NDView<uint16_t, 3> data, const TrackingSetup &setup,
                 const std::vector<size_t> &part_ends, bool update_between) {
    std::optional<NDView<const bool, 2>> mask;
    if (setup.mask)
        mask = setup.mask->view();
    Hist hist(static_cast<size_t>(data.shape(1)),
              static_cast<size_t>(data.shape(2)), setup.nbins, setup.xmin,
              setup.xmax, setup.n_sigma, mask, setup.n_samples,
              setup.batch_size, setup.window);

    if (before.shape(0) > 0)
        hist.fill_histogram(before);
    for (ssize_t i = 0; i < seed.shape(0); ++i)
        hist.push_pedestal_no_update(frame_of(seed, i));
    hist.update_mean();

    std::vector<TrackingSnapshot> snapshots;
    size_t begin = 0;
    for (size_t end : part_ends) {
        hist.fill_histogram(data.sub_view(static_cast<ssize_t>(begin),
                                          static_cast<ssize_t>(end)));
        snapshots.push_back(
            {hist.values(), hist.pedestal_mean(), hist.pedestal_std()});
        if (update_between)
            hist.update_mean();
        begin = end;
    }
    return snapshots;
}

TrackingErrors tracking_errors() {
    const ssize_t nrows = 2, ncols = 3;
    NDArray<uint16_t, 2> frame({nrows, ncols}, 100);
    NDArray<uint16_t, 2> wrong({ncols, nrows}, 100);
    NDArray<bool, 2> wrong_mask({ncols, nrows}, false);

    TrackingErrors r{};
    Hist hist(nrows, ncols, 10, -5.0f, 5.0f, 1.0f, std::nullopt,
              /*n_samples=*/3);
    r.update_mean_before_seed =
        throws<std::runtime_error>([&] { hist.update_mean(); });
    r.pedestal_mean_before_seed =
        throws<std::runtime_error>([&] { hist.pedestal_mean(); });
    r.push_wrong_shape = throws<std::invalid_argument>(
        [&] { hist.push_pedestal_no_update(wrong.view()); });
    for (int i = 0; i < 3; ++i)
        hist.push_pedestal_no_update(frame.view());
    r.frames_after_seed = hist.pedestal_frames();
    r.push_after_seed = throws<std::runtime_error>(
        [&] { hist.push_pedestal_no_update(frame.view()); });
    const auto std0 = hist.pedestal_std();
    r.std_zero_before_update = true;
    for (ssize_t i = 0; i < std0.size(); ++i)
        r.std_zero_before_update &= std0[i] == 0.0f;

    r.zero_n_samples = throws<std::invalid_argument>(
        [&] { Hist h(nrows, ncols, 10, -5.0f, 5.0f, 1.0f, std::nullopt, 0); });
    r.mask_wrong_shape = throws<std::invalid_argument>([&] {
        Hist h(nrows, ncols, 10, -5.0f, 5.0f, 1.0f,
               NDView<const bool, 2>(wrong_mask.view()));
    });
    return r;
}

} // namespace aare::test_cuda
