#include "aare/DataSetAggregation.hpp"

#include <pybind11/pybind11.h>

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

using namespace aare;

template <typename FRAME_TYPE>
void bind_DataSetAggregation(pybind11::module_ &m) {
    m.def(
        "mean_over_dataset",
        [](const std::filesystem::path &path, size_t num_samples,
           size_t num_frames_per_sample, int num_threads) {
            auto result =
                new NDArray<FRAME_TYPE, 3>(mean_over_dataset<FRAME_TYPE>(
                    path, num_samples, num_frames_per_sample, num_threads));
            return return_image_data(result);
        },
        py::arg("path"), py::arg("num_samples"),
        py::arg("num_frames_per_sample"), py::arg("num_threads") = 16,
        R"pbdoc(
        Compute the mean over several frames per sample stored in a file.

        Parameters:
        -----------
            path (str): The path to the file containing the dataset.
            num_samples (int): The number of samples in the dataset.
            num_frames_per_sample (int): The number of frames per sample.
            num_threads (int, optional): The number of threads to use for parallel computation. Defaults to 16.

        Returns:
        --------
            np.ndarray: A numpy array containing the mean frame for each sample with shape (rows, cols, num_samples).
    )pbdoc");
}