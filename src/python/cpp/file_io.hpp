#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/ClusterFileOld.hpp"
#include "aare/file_io/File.hpp"
#include <cstdint>
#include <filesystem>
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;

void define_file_io_bindings(py::module &m) {

    py::class_<File>(m, "File")
        .def(py::init([](const std::filesystem::path &fname) { return File(fname, "r", {}); }))
        .def(py::init([](const std::filesystem::path &fname, const std::string &mode) {
            return File(fname, mode, {});
        }))
        .def(py::init<const std::filesystem::path &, const std::string &, const FileConfig &>())
        .def("read", py::overload_cast<>(&File::read))
        .def("read", py::overload_cast<size_t>(&File::read))
        .def("iread", py::overload_cast<size_t>(&File::iread))
        .def("frame_number", &File::frame_number)
        .def_property_readonly("bytes_per_frame", &File::bytes_per_frame)
        .def_property_readonly("pixels_per_frame", &File::pixels_per_frame)
        .def("seek", &File::seek)
        .def("tell", &File::tell)
        .def_property_readonly("total_frames", &File::total_frames)
        .def_property_readonly("rows", &File::rows)
        .def_property_readonly("cols", &File::cols)
        .def_property_readonly("bitdepth", &File::bitdepth)
        .def_property_readonly("detector_type", &File::detector_type)
        .def_property_readonly(
            "geometry", &File::geometry,
            py::call_guard<py::scoped_ostream_redirect, py::scoped_estream_redirect>())
        .def("set_total_frames", &File::set_total_frames);

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
        .def("__repr__", [](const FileConfig &a) { return "<FileConfig: " + a.to_string() + ">"; });

    py::class_<deprecated::ClusterHeader>(m, "ClusterHeader")
        .def(py::init<>())
        .def_readwrite("frame_number", &deprecated::ClusterHeader::frame_number)
        .def_readwrite("n_clusters", &deprecated::ClusterHeader::n_clusters)
        .def("__repr__", [](const deprecated::ClusterHeader &a) {
            return "<ClusterHeader: " + a.to_string() + ">";
        });

    py::class_<deprecated::Cluster_>(m, "Cluster_")
        .def(py::init<>())
        .def_readwrite("x", &deprecated::Cluster_::x)
        .def_readwrite("y", &deprecated::Cluster_::y)
        .def_readwrite("data", &deprecated::Cluster_::data)
        .def("__repr__", [](const deprecated::Cluster_ &a) {
            return "<Cluster_: " + a.to_string(false) + ">";
        });

    py::class_<deprecated::ClusterV2>(m, "ClusterV2")
        .def(py::init<>())
        .def_readwrite("cluster", &deprecated::ClusterV2::cluster)
        .def_readwrite("frame_number", &deprecated::ClusterV2::frame_number)
        .def("__repr__",
             [](const deprecated::ClusterV2 &a) { return "<ClusterV2: " + a.to_string() + ">"; });

    py::class_<deprecated::ClusterFile>(m, "ClusterFileOld")
        .def(py::init<const std::filesystem::path &, const std::string &>())
        .def("read", py::overload_cast<>(&deprecated::ClusterFile::read))
        .def("read", py::overload_cast<int>(&deprecated::ClusterFile::read))
        .def("frame_number", &deprecated::ClusterFile::frame_number)
        .def("write", py::overload_cast<std::vector<deprecated::ClusterV2> const &>(
                          &deprecated::ClusterFile::write))

        .def("close", &deprecated::ClusterFile::close);

    m.def("to_clustV2", [](std::vector<deprecated::Cluster> &clusters, const int frame_number) {
        std::vector<deprecated::ClusterV2> clusters_;
        for (auto &c : clusters) {
            deprecated::ClusterV2 cluster;
            cluster.cluster.x = c.x;
            cluster.cluster.y = c.y;
            int i = 0;
            for (auto &d : cluster.cluster.data) {
                d = c.get<double>(i++);
            }
            cluster.frame_number = frame_number;
            clusters_.push_back(cluster);
        }
        return clusters_;
    });
}
