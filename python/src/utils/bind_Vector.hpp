#pragma once
#include <pybind11/pybind11.h>
// #include <pybind11/stl.h>
// #include <pybind11/stl_bind.h>
#include <vector>

namespace py = pybind11;

// TODO add index, itemize
template <typename Type>
void define_Vector(py::module &m, const std::string &type_str,
                   const std::string &format_string) {
    auto class_name = "Vector_" + type_str;

    py::class_<std::vector<Type>>(m, class_name.c_str(), py::buffer_protocol())

        .def(py::init())

        .def_buffer([&format_string](
                        std::vector<Type> &self) -> py::buffer_info {
            return py::buffer_info(
                self.data(), static_cast<py::ssize_t>(sizeof(Type)),
                format_string, static_cast<py::ssize_t>(1),
                std::vector<py::ssize_t>{static_cast<py::ssize_t>(self.size())},
                std::vector<py::ssize_t>{
                    static_cast<py::ssize_t>(sizeof(Type))});
        });
}

// fmt::format("T{{{}:sum:i:index}}",py::format_descriptor<Type>::format())