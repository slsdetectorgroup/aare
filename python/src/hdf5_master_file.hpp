
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/Hdf5File.hpp"
#include "aare/Hdf5MasterFile.hpp"

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

void define_hdf5_master_file_bindings(py::module &m) {
    py::class_<Hdf5MasterFile>(m, "Hdf5MasterFile")
        .def(py::init<const std::filesystem::path &>())
        .def("data_fname", &Hdf5MasterFile::data_fname, R"(

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
        .def_property_readonly("version", &Hdf5MasterFile::version)
        .def_property_readonly("detector_type", &Hdf5MasterFile::detector_type)
        .def_property_readonly("timing_mode", &Hdf5MasterFile::timing_mode)
        .def_property_readonly("image_size_in_bytes",
                               &Hdf5MasterFile::image_size_in_bytes)
        .def_property_readonly("frames_in_file",
                               &Hdf5MasterFile::frames_in_file)
        .def_property_readonly("pixels_y", &Hdf5MasterFile::pixels_y)
        .def_property_readonly("pixels_x", &Hdf5MasterFile::pixels_x)
        .def_property_readonly("max_frames_per_file",
                               &Hdf5MasterFile::max_frames_per_file)
        .def_property_readonly("bitdepth", &Hdf5MasterFile::bitdepth)
        .def_property_readonly("frame_padding", &Hdf5MasterFile::frame_padding)
        .def_property_readonly("frame_discard_policy",
                               &Hdf5MasterFile::frame_discard_policy)

        .def_property_readonly("total_frames_expected",
                               &Hdf5MasterFile::total_frames_expected)
        .def_property_readonly("geometry", &Hdf5MasterFile::geometry)
        .def_property_readonly("analog_samples",
                               &Hdf5MasterFile::analog_samples, R"(
            Number of analog samples

            Returns
            ----------
                int | None
                    The number of analog samples in the file (or None if not enabled)
            )")
        .def_property_readonly("digital_samples",
                               &Hdf5MasterFile::digital_samples, R"(
            Number of digital samples

            Returns
            ----------
                int | None
                    The number of digital samples in the file (or None if not enabled)
            )")

        .def_property_readonly("transceiver_samples",
                               &Hdf5MasterFile::transceiver_samples)
        .def_property_readonly("number_of_rows",
                               &Hdf5MasterFile::number_of_rows)
        .def_property_readonly("quad", &Hdf5MasterFile::quad);
    //.def_property_readonly("scan_parameters",
    //                       &Hdf5MasterFile::scan_parameters)
    //.def_property_readonly("roi", &Hdf5MasterFile::roi);
}
