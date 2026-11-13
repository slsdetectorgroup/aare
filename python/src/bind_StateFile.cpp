// SPDX-License-Identifier: MPL-2.0
#include "bindings.hpp"

#include "aare/StateFile.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;

void bind_state_files(py::module &m) {
    m.def(
        "read_state_header",
        [](const std::filesystem::path &filename) {
            return py::module_::import("json").attr("loads")(
                aare::read_state_header(filename));
        },
        py::arg("filename"),
        "Return the JSON header of a state file as a dict, without reading "
        "the arrays: format_version, aare_version, type and state, in which "
        "each array appears as its description (dtype, shape and offset) in "
        "place of its data. Raises RuntimeError if the file is not a state "
        "file, was written in a newer format, or its header is truncated or "
        "damaged.");
}
