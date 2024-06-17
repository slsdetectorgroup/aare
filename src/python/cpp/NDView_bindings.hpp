#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include "aare/core/NDView.hpp"
#include "aare/core/DType.hpp"


namespace py = pybind11;
using namespace aare;
using namespace std;

template <typename ArrayType, ssize_t Ndim>
void define_NDView_bindings(py::module_ &m)
{
    std::string name= "NDView_"+DType(typeid(ArrayType)).to_string()+"_"  +to_string(Ndim);

    py::class_<NDView<ArrayType, Ndim>>(m, name.c_str(), py::buffer_protocol())
        .def(py::init([](py::array_t<ArrayType, py::array::c_style | py::array::forcecast> &np_array)
                      {
                          py::buffer_info info = np_array.request();
                          if (info.format != py::format_descriptor<ArrayType>::format())
                              throw std::runtime_error("Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
                          if (info.ndim != Ndim)
                              throw std::runtime_error("Incompatible dimension: expected a"+ to_string(Ndim) +" array!");

                          std::array<ssize_t, Ndim> arr_shape;
                          std::move(info.shape.begin(), info.shape.end(), arr_shape.begin());

                          NDView<ArrayType, Ndim> a(a.data(),arr_shape);
                          return a; }))
        .def("__getitem__", [](NDView<ArrayType, Ndim> &a, py::tuple index)
             {
                if (index.size() != 2){
                    throw std::runtime_error("Index must be a tuple of size "+to_string(Ndim));
                }
                auto offset =0;
                for(size_t i=0;i<Ndim;i++){
                   offset+=index[i].cast<ssize_t>()*a.strides()[i];
                }
                return a(offset); })

        .def("shape", [](NDView<ArrayType, Ndim> &a)
             { return a.shape(); })
        .def("size", &NDView<ArrayType, Ndim>::size)
        .def_buffer([](NDView<ArrayType, Ndim> &a) -> py::buffer_info
                    { return py::buffer_info(
                          a.data(),                                   /* Pointer to buffer */
                          sizeof(ArrayType),                          /* Size of one scalar */
                          py::format_descriptor<ArrayType>::format(), /* Python struct-style format descriptor */
                          Ndim,                                       /* Number of dimensions */
                          a.shape(),                                  /* Buffer dimensions */
                          a.strides()                                 /* Strides (in bytes) for each index */
                      ); });
}
