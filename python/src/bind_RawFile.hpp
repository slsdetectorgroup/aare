// SPDX-License-Identifier: MPL-2.0
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

void define_raw_file_io_bindings(py::module &m) {
    py::class_<RawFile>(m, "RawFile")
        .def(py::init<const std::filesystem::path &>())
        .def("read_frame",
             [](RawFile &self) {
                 if (self.n_modules_in_roi().size() > 1) {
                     throw std::runtime_error(
                         "File contains multiple ROIs - use read_ROIs()");
                 }
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());

                 // return headers from all subfiles
                 py::array_t<DetectorHeader> header(self.n_modules());

                 const uint8_t item_size = self.bytes_per_pixel();
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()),
                     header.mutable_data());

                 return py::make_tuple(header, image);
             })
        .def(
            "read_n",
            [](RawFile &self, size_t n_frames) {
                if (self.n_modules_in_roi().size() > 1) {
                    throw std::runtime_error(
                        "File contains multiple ROIs - use read_n_ROIs() to "
                        "read a specific ROI or use read_ROIs and "
                        "read one frame at a time.");
                }
                // adjust for actual frames left in the file
                n_frames =
                    std::min(n_frames, self.total_frames() - self.tell());
                if (n_frames == 0) {
                    throw std::runtime_error("No frames left in file");
                }
                std::vector<size_t> shape{n_frames, self.rows(), self.cols()};

                // return headers from all subfiles
                py::array_t<DetectorHeader> header;
                if (self.n_modules() == 1) {
                    header = py::array_t<DetectorHeader>(n_frames);
                } else {
                    header = py::array_t<DetectorHeader>(
                        {self.n_modules_in_roi()[0], n_frames});
                }
                // py::array_t<DetectorHeader> header({self.n_mod(), n_frames});

                py::array image;
                const uint8_t item_size = self.bytes_per_pixel();
                if (item_size == 1) {
                    image = py::array_t<uint8_t>(shape);
                } else if (item_size == 2) {
                    image = py::array_t<uint16_t>(shape);
                } else if (item_size == 4) {
                    image = py::array_t<uint32_t>(shape);
                }
                self.read_into(
                    reinterpret_cast<std::byte *>(image.mutable_data()),
                    n_frames, header.mutable_data());

                return py::make_tuple(header, image);
            },
            R"(
             Read n frames from the file.
             )")

        .def(
            "read_ROIs",
            [](RawFile &self,
               const std::optional<size_t> roi_index = std::nullopt) {
                if (self.num_rois() == 0) {
                    throw std::runtime_error(LOCATION + "No ROIs defined.");
                }

                if (roi_index.has_value() &&
                    roi_index.value() >= self.num_rois()) {
                    throw std::runtime_error(LOCATION +
                                             "ROI index out of range.");
                }

                size_t number_of_ROIs =
                    roi_index.has_value() ? 1 : self.num_rois();

                const uint8_t item_size = self.bytes_per_pixel();

                std::vector<py::array> images(number_of_ROIs);

                for (size_t r = 0; r < number_of_ROIs; r++) {
                    std::vector<ssize_t> shape;
                    shape.reserve(2);
                    shape.push_back(self.roi_geometries(r).pixels_y());
                    shape.push_back(self.roi_geometries(r).pixels_x());

                    if (item_size == 1) {
                        images[r] = py::array_t<uint8_t>(shape);
                    } else if (item_size == 2) {
                        images[r] = py::array_t<uint16_t>(shape);
                    } else if (item_size == 4) {
                        images[r] = py::array_t<uint32_t>(shape);
                    }

                    const size_t roi_idx =
                        roi_index.has_value() ? roi_index.value() : r;
                    self.read_roi_into(
                        reinterpret_cast<std::byte *>(images[r].mutable_data()),
                        roi_idx, self.tell());
                }
                self.seek(self.tell() + 1); // advance frame number so the
                return images;
            },
            R"(
            Read all ROIs for the current frame. 
            param: 
                roi_index: optional index of the ROI to read. If not provided, all ROIs are read.
            Note: The method advances the frame number so reading ROIs one after the other won't work.
            Returns a list of numpy arrays, one for each ROI.
            )")

        .def(
            "read_ROIs",
            [](RawFile &self, const size_t frame_number,
               const std::optional<size_t> roi_index = std::nullopt) {
                if (self.num_rois() == 0) {
                    throw std::runtime_error(LOCATION + "No ROIs defined.");
                }

                if (roi_index.has_value() &&
                    roi_index.value() >= self.num_rois()) {
                    throw std::runtime_error(LOCATION +
                                             "ROI index out of range.");
                }

                size_t number_of_ROIs =
                    roi_index.has_value() ? 1 : self.num_rois();

                const uint8_t item_size = self.bytes_per_pixel();

                std::vector<py::array> images(number_of_ROIs);

                self.seek(frame_number);

                for (size_t r = 0; r < number_of_ROIs; r++) {
                    std::vector<ssize_t> shape;
                    shape.reserve(2);
                    shape.push_back(self.roi_geometries(r).pixels_y());
                    shape.push_back(self.roi_geometries(r).pixels_x());

                    if (item_size == 1) {
                        images[r] = py::array_t<uint8_t>(shape);
                    } else if (item_size == 2) {
                        images[r] = py::array_t<uint16_t>(shape);
                    } else if (item_size == 4) {
                        images[r] = py::array_t<uint32_t>(shape);
                    }

                    const size_t roi_idx =
                        roi_index.has_value() ? roi_index.value() : r;
                    self.read_roi_into(
                        reinterpret_cast<std::byte *>(images[r].mutable_data()),
                        roi_idx, self.tell());
                }
                self.seek(self.tell() + 1); // advance frame number so the
                return images;
            },
            R"(
            Read all ROIs for the current frame. 
            param: 
                frame_number: frame number to read.
                roi_index: optional index of the ROI to read. If not provided, all ROIs are read.
            Note: The method advances the frame number so reading ROIs one after the other won't work.
            Returns a list of numpy arrays, one for each ROI.
            )")

        .def(
            "read_n_ROIs",
            [](RawFile &self, const size_t num_frames, const size_t roi_index) {
                if (self.num_rois() == 0) {
                    throw std::runtime_error(LOCATION + "No ROIs defined.");
                }

                if (roi_index >= self.num_rois()) {
                    throw std::runtime_error(LOCATION +
                                             "ROI index out of range.");
                }

                // adjust for actual frames left in the file
                size_t n_frames =
                    std::min(num_frames, self.total_frames() - self.tell());
                if (n_frames == 0) {
                    throw std::runtime_error("No frames left in file");
                }
                std::vector<size_t> shape{
                    n_frames, self.roi_geometries(roi_index).pixels_y(),
                    self.roi_geometries(roi_index).pixels_x()};

                py::array image;
                const uint8_t item_size = self.bytes_per_pixel();
                if (item_size == 1) {
                    image = py::array_t<uint8_t>(shape);
                } else if (item_size == 2) {
                    image = py::array_t<uint16_t>(shape);
                } else if (item_size == 4) {
                    image = py::array_t<uint32_t>(shape);
                }

                auto image_buffer =
                    reinterpret_cast<std::byte *>(image.mutable_data());
                for (size_t i = 0; i < n_frames; i++) {
                    self.read_roi_into(image_buffer, roi_index, self.tell());

                    self.seek(self.tell() + 1); // advance frame number
                    image_buffer += self.bytes_per_frame(roi_index);
                }

                return image;
            },
            R"(
             Read n frames for the given ROI index.
             param: 
                    n_frames: number of frames to read.
                    roi_index: index of the ROI to read.
             Returns a numpy array containing the frames for the specified ROI.
             )")

        .def("frame_number", &RawFile::frame_number)
        .def_property_readonly(
            "bytes_per_frame",
            static_cast<size_t (RawFile::*)()>(&RawFile::bytes_per_frame))
        .def(
            "bytes_per_frame",
            [](RawFile &self, const size_t roi_index) {
                return self.bytes_per_frame(roi_index);
            },
            R"(
            Bytes per frame for the given ROI.
            )")
        .def_property_readonly(
            "pixels_per_frame",
            static_cast<size_t (RawFile::*)()>(&RawFile::pixels_per_frame))
        .def(
            "pixels_per_frame",
            [](RawFile &self, const size_t roi_index) {
                return self.pixels_per_frame(roi_index);
            },
            R"(
            Pixels per frame for the given ROI.
            )")
        .def_property_readonly("bytes_per_pixel", &RawFile::bytes_per_pixel)
        .def("seek", &RawFile::seek, R"(
            Seek to a frame index in file.
            )")
        .def("tell", &RawFile::tell, R"(
            Return the current frame number.)")
        .def_property_readonly("total_frames", &RawFile::total_frames)
        .def_property_readonly(
            "rows", static_cast<size_t (RawFile::*)() const>(&RawFile::rows))
        .def(
            "rows",
            [](RawFile &self, const size_t roi_index) {
                return self.rows(roi_index);
            },
            R"(
            Rows for the given ROI.
            )")
        .def_property_readonly(
            "cols", static_cast<size_t (RawFile::*)() const>(&RawFile::cols))
        .def(
            "cols",
            [](RawFile &self, const size_t roi_index) {
                return self.cols(roi_index);
            },
            R"(
            Cols for the given ROI.
            )")
        .def_property_readonly("bitdepth", &RawFile::bitdepth)
        .def_property_readonly("geometry", &RawFile::geometry)
        .def_property_readonly("detector_type", &RawFile::detector_type)
        .def_property_readonly("master", &RawFile::master)
        .def_property_readonly("n_modules", &RawFile::n_modules)
        .def_property_readonly("n_modules_in_roi", &RawFile::n_modules_in_roi)
        .def_property_readonly("num_rois", &RawFile::num_rois);
}