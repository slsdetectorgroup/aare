#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/ClusterFileV2.hpp"
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
        .def(
            py::init([](const std::filesystem::path &fname, const std::string &mode) { return File(fname, mode, {}); }))
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
        .def_property_readonly("geometry", &File::geometry,
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

    py::class_<ClusterHeader>(m, "ClusterHeader")
        .def(py::init<>())
        .def_readwrite("frame_number", &ClusterHeader::frame_number)
        .def_readwrite("n_clusters", &ClusterHeader::n_clusters)
        .def("__repr__", [](const ClusterHeader &a) { return "<ClusterHeader: " + a.to_string() + ">"; });

    py::class_<Cluster_>(m, "Cluster_")
        .def(py::init<>())
        .def_readwrite("x", &Cluster_::x)
        .def_readwrite("y", &Cluster_::y)
        .def_readwrite("data", &Cluster_::data)
        .def("__repr__", [](const Cluster_ &a) { return "<Cluster_: " + a.to_string(false) + ">"; });

    py::class_<ClusterV2>(m, "ClusterV2")
        .def(py::init<>())
        .def_readwrite("cluster", &ClusterV2::cluster)
        .def_readwrite("frame_number", &ClusterV2::frame_number)
        .def("__repr__", [](const ClusterV2 &a) { return "<ClusterV2: " + a.to_string() + ">"; });

    py::class_<ClusterFileV2>(m, "ClusterFileV2")
        .def(py::init<const std::filesystem::path &, const std::string &>())
        .def("read", py::overload_cast<>(&ClusterFileV2::read))
        .def("read", py::overload_cast<int>(&ClusterFileV2::read))
        .def("frame_number", &ClusterFileV2::frame_number)
        .def("write", py::overload_cast<std::vector<ClusterV2> const &>(&ClusterFileV2::write))
        
        .def("close", &ClusterFileV2::close);

    m.def("to_clustV2", [](std::vector<Cluster> &clusters, const int frame_number) {
        std::vector<ClusterV2> clusters_;
        for (auto &c : clusters) {
            ClusterV2 cluster;
            cluster.cluster.x = c.x;
            cluster.cluster.y = c.y;
            int i=0;
            for(auto &d : cluster.cluster.data) {
                d=c.get<double>(i++);
            }
            cluster.frame_number = frame_number;
            clusters_.push_back(cluster);
        }
        return clusters_;
    });
}
