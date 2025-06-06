Requirements
==============================================

- C++17 compiler (gcc 8/clang 7)
- CMake 3.15+

**Internally used libraries**

.. note ::

    To save compile time some of the dependencies can also be picked up from the system/conda environment by specifying:
    -DAARE_SYSTEM_LIBRARIES=ON during the cmake configuration.

To simplify deployment we build and statically link a few libraries.

- fmt
- lmfit - https://jugit.fz-juelich.de/mlz/lmfit
- nlohmann_json
- pybind11
- ZeroMQ

**Extra dependencies for building documentation**

- Sphinx
- Breathe
- Doxygen