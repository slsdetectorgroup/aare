// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FastPedestal.hpp"
#include "aare/StateFile.hpp"

#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>

/**
 * Bind save_state(filename) and load_state(filename) for a class with
 * state() and set_state(State). load_state is an instance method that loads
 * into the object it is called on: as a static method that returns a new
 * object it could be called on an existing one, which then stayed unchanged.
 * The static from_state_file() takes the constructor arguments of its class
 * and is bound there.
 */
template <typename State, typename Class, typename... Options>
void def_state_file(pybind11::class_<Class, Options...> &cls,
                    const char *save_doc, const char *load_doc) {
    cls.def(
           "save_state",
           [](const Class &self, const std::filesystem::path &filename) {
               aare::save_state(self.state(), filename);
           },
           pybind11::arg("filename"), save_doc)
        .def(
            "load_state",
            [](Class &self, const std::filesystem::path &filename) {
                self.set_state(aare::load_state<State>(filename));
            },
            pybind11::arg("filename"), load_doc);
}

template <typename T> struct type_tag {
    using type = T;
};

// Calls f once for each FastPedestal type bound in bind_FastPedestal.cpp,
// to bind an overload for each
template <typename F> void for_each_fast_pedestal_type(F &&f) {
    f(type_tag<aare::FastPedestal<double>>{});
    f(type_tag<aare::FastPedestal<float>>{});
    f(type_tag<aare::FastPedestal<int16_t>>{});
}
