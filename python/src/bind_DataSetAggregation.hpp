#include "aare/DataSetAggregation.hpp"

#include <pybind11/pybind11.h>

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

using namespace aare;

template <typename FRAME_TYPE>
void bind_DataSetAggregation(pybind11::module_ &m) {
    m.def(
        "block_mean",
        [](const std::filesystem::path &path, const size_t num_samples,
           const size_t mean_over_num_frames,
           const size_t num_processing_threads,
           const size_t num_reader_threads) {
            auto result = new NDArray<double, 3>(block_mean<FRAME_TYPE>(
                path, num_samples, mean_over_num_frames, num_processing_threads,
                num_reader_threads));
            return return_image_data(result);
        },
        py::arg("path"), py::kw_only(), py::arg("num_samples"),
        py::arg("mean_over_num_frames"), py::arg("num_processing_threads") = 12,
        py::arg("num_reader_threads") = 8,
        R"pbdoc(
        Compute the block mean over several frames per sample stored in a file.
        Function spawns num_processing_threads + num_reader_threads + 1 threads in total.

        Parameters:
        -----------
            path (str): The path to the file containing the dataset.
            num_samples (int): The number of samples in the dataset.
            mean_over_num_frames (int): The number of frames per sample to take mean over.
            num_processing_threads (int, optional): The number of threads to use for parallel computation. One thread reads, the others compute the mean. Defaults to 12.
            num_reader_threads (int, optional): The number of file workers the reader thread uses to load each sample. Defaults to 8.

        Returns:
        --------
            np.ndarray: A numpy array containing the mean frame stored as np.float64 for each sample with shape (rows, cols, num_samples).
    )pbdoc");
}

void bind_read_current_source(pybind11::module_ &m) {
    m.def(
        "mean_current_source",
        [](const std::filesystem::path &path, const size_t num_samples,
           const size_t mean_over_num_frames,
           const size_t num_processing_threads, const size_t num_reader_threads,
           const size_t file_offset) {
            auto result = new NDArray<double, 3>(mean_current_source(
                path, num_samples, mean_over_num_frames, num_processing_threads,
                num_reader_threads, file_offset));
            return return_image_data(result);
        },
        py::arg("path"), py::kw_only(), py::arg("num_samples"),
        py::arg("mean_over_num_frames"), py::arg("num_processing_threads") = 12,
        py::arg("num_reader_threads") = 8, py::arg("file_offset") = 0,
        R"pbdoc(
        Compute the mean over several frames per sample stored in a file for each current source.
        Function spawns num_processing_threads + num_reader_threads + 1 threads in total.

        Parameters:
        -----------
            path (str): The path to the file containing the dataset.
            num_samples (int): The number of sources/samples in the dataset.
            mean_over_num_frames (int): The number of frames per sample to take mean over.
            num_processing_threads (int, optional): The number of threads to use for parallel computation. One thread reads, the others each process a shard of rows. Defaults to 12.
            num_reader_threads (int, optional): The number of file workers the reader thread uses to load the frames. Defaults to 8.
            file_offset (int, optional): The offset in the file to start reading from. Defaults to 0.

        Returns:
        --------
            np.ndarray: A numpy array containing the mean frame stored as np.float64 for each current source with shape (rows, cols, num_sources).
    )pbdoc");
}