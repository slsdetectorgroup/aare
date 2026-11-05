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
- Minuit2
- nlohmann_json
- pybind11
- ZeroMQ

**Extra dependencies for the CUDA backend (-DAARE_CUDA=ON)**

Only needed to build :doc:`ClusterFinderCUDA`; aare builds CPU-only without them.

- CUDA toolkit 11.0 or newer (nvcc), and a host compiler that nvcc supports.
  The device code is compiled as CUDA C++17, which nvcc supports from CUDA 11.
- CMake 3.24+ if you keep the default ``AARE_CUDA_ARCHITECTURES=native``;
  ``native`` is not understood by older CMake. With an explicit architecture
  list the project minimum of 3.15 is enough.
- An NVIDIA GPU at runtime.

**Extra dependencies for building documentation**

- Sphinx
- Breathe
- Doxygen
