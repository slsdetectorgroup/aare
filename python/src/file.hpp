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

void define_file_io_bindings(py::module &m) {


    py::enum_<DetectorType>(m, "DetectorType")
        .value("Jungfrau", DetectorType::Jungfrau)
        .value("Eiger", DetectorType::Eiger)
        .value("Mythen3", DetectorType::Mythen3)
        .value("Moench", DetectorType::Moench)
        .value("Moench03", DetectorType::Moench03)
        .value("Moench03_old", DetectorType::Moench03_old)
        .value("ChipTestBoard", DetectorType::ChipTestBoard)
        .value("Unknown", DetectorType::Unknown);


    PYBIND11_NUMPY_DTYPE(DetectorHeader, frameNumber, expLength, packetNumber,
                         bunchId, timestamp, modId, row, column, reserved,
                         debug, roundRNumber, detType, version, packetMask);

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

    py::class_<File>(m, "File")
        .def(py::init([](const std::filesystem::path &fname) {
            return File(fname, "r", {});
        }))
        .def(py::init(
            [](const std::filesystem::path &fname, const std::string &mode) {
                return File(fname, mode, {});
            }))
        .def(py::init<const std::filesystem::path &, const std::string &,
                      const FileConfig &>())

        .def("frame_number", &File::frame_number)
        .def_property_readonly("bytes_per_frame", &File::bytes_per_frame)
        .def_property_readonly("pixels_per_frame", &File::pixels_per_frame)
        .def("seek", &File::seek)
        .def("tell", &File::tell)
        .def_property_readonly("total_frames", &File::total_frames)
        .def_property_readonly("rows", &File::rows)
        .def_property_readonly("cols", &File::cols)
        .def_property_readonly("bitdepth", &File::bitdepth)
        .def_property_readonly("bytes_per_pixel", &File::bytes_per_pixel)
        .def_property_readonly(
            "detector_type",
            [](File &self) { return ToString(self.detector_type()); })
        .def("read_frame",
             [](File &self) {
                 const uint8_t item_size = self.bytes_per_pixel();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()));
                 return image;
             })
        .def("read_frame",
             [](File &self, size_t frame_number) {
                 self.seek(frame_number);
                 const uint8_t item_size = self.bytes_per_pixel();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()));
                 return image;
             })
        .def("read_n", [](File &self, size_t n_frames) {
            const uint8_t item_size = self.bytes_per_pixel();
            py::array image;
            std::vector<ssize_t> shape;
            shape.reserve(3);
            shape.push_back(n_frames);
            shape.push_back(self.rows());
            shape.push_back(self.cols());
            if (item_size == 1) {
                image = py::array_t<uint8_t>(shape);
            } else if (item_size == 2) {
                image = py::array_t<uint16_t>(shape);
            } else if (item_size == 4) {
                image = py::array_t<uint32_t>(shape);
            }
            self.read_into(reinterpret_cast<std::byte *>(image.mutable_data()),
                           n_frames);
            return image;
        });

    py::class_<FileConfig>(m, "FileConfig")
        .def(py::init<>())
        .def_readwrite("rows", &FileConfig::rows)
        .def_readwrite("cols", &FileConfig::cols)
        .def_readwrite("version", &FileConfig::version)
        .def_readwrite("geometry", &FileConfig::geometry)
        .def_readwrite("detector_type", &FileConfig::detector_type)
        .def_readwrite("max_frames_per_file", &FileConfig::max_frames_per_file)
        .def_readwrite("total_frames", &FileConfig::total_frames)
        .def_readwrite("dtype", &FileConfig::dtype)
        .def("__eq__", &FileConfig::operator==)
        .def("__ne__", &FileConfig::operator!=)
        .def("__repr__", [](const FileConfig &a) {
            return "<FileConfig: " + a.to_string() + ">";
        });



    py::class_<ScanParameters>(m, "ScanParameters")
        .def(py::init<const std::string &>())
        .def(py::init<const ScanParameters &>())

        .def_property_readonly("enabled", &ScanParameters::enabled)
        .def_property_readonly("dac", &ScanParameters::dac)
        .def_property_readonly("start", &ScanParameters::start)
        .def_property_readonly("stop", &ScanParameters::stop)
        .def_property_readonly("step", &ScanParameters::step);


    py::class_<ROI>(m, "ROI")
        .def(py::init<>())
        .def_readwrite("xmin", &ROI::xmin)
        .def_readwrite("xmax", &ROI::xmax)
        .def_readwrite("ymin", &ROI::ymin)
        .def_readwrite("ymax", &ROI::ymax)
        .def("__str__", [](const ROI& self){
            return fmt::format("ROI: xmin: {} xmax: {} ymin: {} ymax: {}", self.xmin, self.xmax, self.ymin, self.ymax);
        })
        .def("__repr__", [](const ROI& self){
            return fmt::format("<ROI: xmin: {} xmax: {} ymin: {} ymax: {}>", self.xmin, self.xmax, self.ymin, self.ymax);
        })
        .def("__iter__", [](const ROI &self) {
            return py::make_iterator(&self.xmin, &self.ymax+1);
        });

    


    py::class_<RawSubFile>(m, "RawSubFile")
        .def(py::init<const std::filesystem::path &, DetectorType, size_t,
                      size_t, size_t>())
        .def_property_readonly("bytes_per_frame", &RawSubFile::bytes_per_frame)
        .def_property_readonly("pixels_per_frame",
                               &RawSubFile::pixels_per_frame)
        .def("seek", &RawSubFile::seek)
        .def("tell", &RawSubFile::tell)
        .def_property_readonly("rows", &RawSubFile::rows)
        .def_property_readonly("cols", &RawSubFile::cols)
        .def("read_frame",
             [](RawSubFile &self) {
                 const uint8_t item_size = self.bytes_per_pixel();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 fmt::print("item_size: {} rows: {} cols: {}\n", item_size, self.rows(), self.cols());
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()));
                 return image;
             });


    // py::class_<ClusterHeader>(m, "ClusterHeader")
    //     .def(py::init<>())
    //     .def_readwrite("frame_number", &ClusterHeader::frame_number)
    //     .def_readwrite("n_clusters", &ClusterHeader::n_clusters)
    //     .def("__repr__", [](const ClusterHeader &a) { return "<ClusterHeader:
    //     " + a.to_string() + ">"; });

    // py::class_<ClusterV2_>(m, "ClusterV2_")
    //     .def(py::init<>())
    //     .def_readwrite("x", &ClusterV2_::x)
    //     .def_readwrite("y", &ClusterV2_::y)
    //     .def_readwrite("data", &ClusterV2_::data)
    //     .def("__repr__", [](const ClusterV2_ &a) { return "<ClusterV2_: " +
    //     a.to_string(false) + ">"; });

    // py::class_<ClusterV2>(m, "ClusterV2")
    //     .def(py::init<>())
    //     .def_readwrite("cluster", &ClusterV2::cluster)
    //     .def_readwrite("frame_number", &ClusterV2::frame_number)
    //     .def("__repr__", [](const ClusterV2 &a) { return "<ClusterV2: " +
    //     a.to_string() + ">"; });

    // py::class_<ClusterFileV2>(m, "ClusterFileV2")
    //     .def(py::init<const std::filesystem::path &, const std::string &>())
    //     .def("read", py::overload_cast<>(&ClusterFileV2::read))
    //     .def("read", py::overload_cast<int>(&ClusterFileV2::read))
    //     .def("frame_number", &ClusterFileV2::frame_number)
    //     .def("write", py::overload_cast<std::vector<ClusterV2> const
    //     &>(&ClusterFileV2::write))

    //     .def("close", &ClusterFileV2::close);

    // m.def("to_clustV2", [](std::vector<DynamicCluster> &clusters, const int
    // frame_number) {
    //     std::vector<ClusterV2> clusters_;
    //     for (auto &c : clusters) {
    //         ClusterV2 cluster;
    //         cluster.cluster.x = c.x;
    //         cluster.cluster.y = c.y;
    //         int i=0;
    //         for(auto &d : cluster.cluster.data) {
    //             d=c.get<double>(i++);
    //         }
    //         cluster.frame_number = frame_number;
    //         clusters_.push_back(cluster);
    //     }
    //     return clusters_;
    // });
}
