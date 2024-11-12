
#include "aare/CtbRawFile.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#include "aare/defs.hpp"
// #include "aare/fClusterFileV2.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>

namespace py = pybind11;
using namespace ::aare;

void define_raw_master_file_bindings(py::module &m) {
    py::class_<RawMasterFile>(m, "RawMasterFile")
        .def(py::init<const std::filesystem::path &>())
        .def("data_fname", &RawMasterFile::data_fname, R"(

            Parameters
            ------------
                module_index : int 
                        module index (d0, d1 .. dN)
                file_index : int 
                        file index (f0, f1 .. fN)

            Returns
            ----------
                os.PathLike
                        The name of the data file.

            )")
        .def_property_readonly("version", &RawMasterFile::version)
        .def_property_readonly("detector_type", &RawMasterFile::detector_type)
        .def_property_readonly("timing_mode", &RawMasterFile::timing_mode)
        .def_property_readonly("image_size_in_bytes",
                               &RawMasterFile::image_size_in_bytes)
        .def_property_readonly("frames_in_file", &RawMasterFile::frames_in_file)
        .def_property_readonly("pixels_y", &RawMasterFile::pixels_y)
        .def_property_readonly("pixels_x", &RawMasterFile::pixels_x)
        .def_property_readonly("max_frames_per_file",
                               &RawMasterFile::max_frames_per_file)
        .def_property_readonly("bitdepth", &RawMasterFile::bitdepth)
        .def_property_readonly("frame_padding", &RawMasterFile::frame_padding)
        .def_property_readonly("frame_discard_policy",
                               &RawMasterFile::frame_discard_policy)

        .def_property_readonly("total_frames_expected",
                               &RawMasterFile::total_frames_expected)
        .def_property_readonly("geometry", &RawMasterFile::geometry)
        .def_property_readonly("analog_samples", &RawMasterFile::analog_samples, R"(
            Number of analog samples

            Returns
            ----------
                int | None
                    The number of analog samples in the file (or None if not enabled)
            )")
        .def_property_readonly("digital_samples",
                               &RawMasterFile::digital_samples,  R"(
            Number of digital samples

            Returns
            ----------
                int | None
                    The number of digital samples in the file (or None if not enabled)
            )")

        .def_property_readonly("transceiver_samples",
                               &RawMasterFile::transceiver_samples)
        .def_property_readonly("number_of_rows", &RawMasterFile::number_of_rows)
        .def_property_readonly("quad", &RawMasterFile::quad)
        .def_property_readonly("scan_parameters",
                               &RawMasterFile::scan_parameters)
        .def_property_readonly("roi", &RawMasterFile::roi);
}
