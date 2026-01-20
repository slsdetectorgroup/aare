
#include "aare/CtbRawFile.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#include "aare/decode.hpp"
#include "aare/defs.hpp"
// #include "aare/fClusterFileV2.hpp"

#include "np_helper.hpp"

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

void define_ctb_raw_file_io_bindings(py::module &m) {

    m.def("adc_sar_05_decode64to16", [](py::array_t<uint8_t> input) {
        if (input.ndim() != 2) {
            throw std::runtime_error(
                "Only 2D arrays are supported at this moment");
        }

        // Create a 2D output array with the same shape as the input
        std::vector<ssize_t> shape{input.shape(0),
                                   input.shape(1) /
                                       static_cast<ssize_t>(bits_per_byte)};
        py::array_t<uint16_t> output(shape);

        // Create a view of the input and output arrays
        NDView<uint64_t, 2> input_view(
            reinterpret_cast<uint64_t *>(input.mutable_data()),
            {output.shape(0), output.shape(1)});
        NDView<uint16_t, 2> output_view(output.mutable_data(),
                                        {output.shape(0), output.shape(1)});

        adc_sar_05_decode64to16(input_view, output_view);

        return output;
    });

    m.def("adc_sar_04_decode64to16", [](py::array_t<uint8_t> input) {
        if (input.ndim() != 2) {
            throw std::runtime_error(
                "Only 2D arrays are supported at this moment");
        }

        // Create a 2D output array with the same shape as the input
        std::vector<ssize_t> shape{input.shape(0),
                                   input.shape(1) /
                                       static_cast<ssize_t>(bits_per_byte)};
        py::array_t<uint16_t> output(shape);

        // Create a view of the input and output arrays
        NDView<uint64_t, 2> input_view(
            reinterpret_cast<uint64_t *>(input.mutable_data()),
            {output.shape(0), output.shape(1)});
        NDView<uint16_t, 2> output_view(output.mutable_data(),
                                        {output.shape(0), output.shape(1)});

        adc_sar_04_decode64to16(input_view, output_view);

        return output;
    });

    m.def("apply_custom_weights",
          [](py::array_t<uint16_t, py::array::c_style | py::array::forcecast>
                 &input,
             py::array_t<double, py::array::c_style | py::array::forcecast>
                 &weights) {
              // Create new array with same shape as the input array
              // (uninitialized values)
              py::buffer_info buf = input.request();
              py::array_t<double> output(buf.shape);

              // Use NDViews to call into the C++ library
              auto weights_view = make_view_1d(weights);
              NDView<uint16_t, 1> input_view(input.mutable_data(),
                                             {input.size()});
              NDView<double, 1> output_view(output.mutable_data(),
                                            {output.size()});

              apply_custom_weights(input_view, output_view, weights_view);
              return output;
          });

    py::class_<CtbRawFile>(m, "CtbRawFile")
        .def(py::init<const std::filesystem::path &>())
        .def("read_frame",
             [](CtbRawFile &self) {
                 size_t image_size = self.image_size_in_bytes();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(1);
                 shape.push_back(image_size);

                 py::array_t<DetectorHeader> header(1);

                 // always read bytes
                 image = py::array_t<uint8_t>(shape);

                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()),
                     header.mutable_data());

                 return py::make_tuple(header, image);
             })
        .def("seek", &CtbRawFile::seek)
        .def("tell", &CtbRawFile::tell)
        .def("master", &CtbRawFile::master)

        .def_property_readonly("image_size_in_bytes",
                               &CtbRawFile::image_size_in_bytes)

        .def_property_readonly("frames_in_file", &CtbRawFile::frames_in_file);
}
