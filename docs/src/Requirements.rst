Requirements
==============================================

- C++17 compiler (gcc 8/clang 7)
- CMake 3.14+

**Internally used libraries**

.. note ::

    These can also be picked up from the system/conda environment by specifying:
    -DAARE_SYSTEM_LIBRARIES=ON during the cmake configuration.

- pybind11
- fmt
- nlohmann_json
- ZeroMQ

**Extra dependencies for building documentation**

- Sphinx
- Breathe
- Doxygen