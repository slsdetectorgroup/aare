// SPDX-License-Identifier: MPL-2.0

#include "aare/CtbRawFile.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#include "aare/decode.hpp"
#include "aare/defs.hpp"

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

// One binding for the three ADC SAR decoders. Each is registered under
// its original Python name so existing transforms keep working.
template <void (*decode)(NDView<const uint8_t, 2>, NDView<uint16_t, 2>)>
py::array_t<uint16_t> decode64to16(py::array_t<uint8_t> input) {
    auto bytes = make_const_view_2d(input);
    constexpr ssize_t bytes_per_word = sizeof(uint64_t);
    auto output = py::array_t<uint16_t>(
        {bytes.shape(0), bytes.shape(1) / bytes_per_word});
    decode(bytes, make_view_2d(output));
    return output;
}

constexpr auto decode64to16_doc =
    "Decode packed 64-bit ADC samples. Takes a two-dimensional, "
    "C-contiguous uint8 array whose row length is a multiple of 8 bytes "
    "and returns a uint16 array with one value per 64-bit word.";

void define_ctb_raw_file_io_bindings(py::module &m) {

    m.def("adc_sar_05_06_07_08decode64to16",
          &decode64to16<adc_sar_05_06_07_08decode64to16>,
          py::arg("input").noconvert(), decode64to16_doc);

    m.def("adc_sar_05_decode64to16", &decode64to16<adc_sar_05_decode64to16>,
          py::arg("input").noconvert(), decode64to16_doc);

    m.def("adc_sar_04_decode64to16", &decode64to16<adc_sar_04_decode64to16>,
          py::arg("input").noconvert(), decode64to16_doc);

    m.def(
        "apply_custom_weights",
        [](py::array_t<uint16_t> input, py::array_t<double> weights) {
            auto input_view = make_view_1d(input);
            auto weights_view = make_view_1d(weights);
            auto output = py::array_t<double>(input_view.size());
            apply_custom_weights(input_view, make_view_1d(output),
                                 weights_view);
            return output;
        },
        py::arg("input").noconvert(), py::arg("weights").noconvert(),
        "Apply per-bit weights to every value of a one-dimensional, "
        "C-contiguous uint16 array and return a float64 array.");

    m.def(
        "expand24to32bit",
        [](py::array_t<uint8_t> input, uint32_t offset) {
            constexpr ssize_t bytes_per_channel = 3; // 24 bit
            auto input_view = make_const_view_1d(input);
            auto output =
                py::array_t<uint32_t>(input_view.size() / bytes_per_channel);
            aare::expand24to32bit(input_view, make_view_1d(output),
                                  aare::BitOffset(offset));
            return output;
        },
        py::arg("input").noconvert(), py::arg("offset"),
        "Expand packed 24-bit values from a one-dimensional, C-contiguous "
        "uint8 array into a uint32 array.");

    m.def(
        "expand4to8bit",
        [](py::array_t<uint8_t> input) {
            auto input_view = make_const_view_1d(input);
            auto output = py::array_t<uint8_t>(input_view.size() * 2);
            aare::expand4to8bit(input_view, make_view_1d(output));
            return output;
        },
        py::arg("input").noconvert(),
        "Expand two 4-bit values per byte of a one-dimensional, C-contiguous "
        "uint8 array into a uint8 array of twice the length.");

    m.def(
        "decode_my302",
        [](py::array_t<uint8_t> input, uint32_t offset) {
            // Physical layout of the chip
            constexpr ssize_t channels = 64;
            constexpr ssize_t counters = 3;
            constexpr ssize_t bytes_per_channel = 3; // 24 bit
            constexpr ssize_t n_outputs = 2;

            auto input_view = make_const_view_1d(input);

            ssize_t expected_size = channels * counters * bytes_per_channel;

            // If we have an offset we need one extra byte per output
            aare::BitOffset bitoff(offset);
            if (bitoff.value())
                expected_size += n_outputs;

            if (input_view.size() != expected_size) {
                throw py::value_error(
                    fmt::format("{} Expected an input size of {} bytes. Called "
                                "with input size of {}",
                                LOCATION, expected_size, input_view.size()));
            }

            auto output = py::array_t<uint32_t>(channels * counters);
            auto output_view = make_view_1d(output);

            const ssize_t in_step = input_view.size() / n_outputs;
            const ssize_t out_step = output_view.size() / n_outputs;
            for (ssize_t i = 0; i != n_outputs; ++i) {
                NDView<const uint8_t, 1> in_part(
                    input_view.data() + in_step * i, {in_step});
                NDView<uint32_t, 1> out_part(output_view.data() + out_step * i,
                                             {out_step});
                aare::expand24to32bit(in_part, out_part, bitoff);
            }

            return output;
        },
        py::arg("input").noconvert(), py::arg("offset"),
        "Decode a Mythen 302 readout from a one-dimensional, C-contiguous "
        "uint8 array into a uint32 array of 64 channels times 3 counters.");

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

        .def_property_readonly("frames_in_file", &CtbRawFile::frames_in_file)
        .def_property_readonly("total_frames", &CtbRawFile::total_frames)
        .def("__len__", &CtbRawFile::total_frames);
}
