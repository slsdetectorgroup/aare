# SPDX-License-Identifier: MPL-2.0

find_package(
  Python 3.10
  COMPONENTS Interpreter Development.Module
  REQUIRED)
set(PYBIND11_FINDPYTHON ON) # Needed for RH8

# Download or find pybind11 depending on configuration
if(AARE_FETCH_PYBIND11)
  FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11
    GIT_TAG v3.0.3
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(pybind11)
else()
  find_package(pybind11 3.0.3 REQUIRED)
endif()

# ---- Main CPU module --------------------------------------------------------
# When AARE_CUDA=ON, the CUDA bindings live in a *separate* Python extension
# (_aare_cuda.so, below) loaded independently at runtime. This isolates the
# nvcc-compiled translation unit into its own ELF image so pybind11's type
# registry cannot be corrupted by weak-symbol collisions between gcc-emitted and
# nvcc-emitted template instantiations.
#
# The compiled python extension. Each bind_*.cpp holds the bindings of one class
# or function group and is its own translation unit, so the units compile in
# parallel and an edit recompiles only its file. The templated cluster bindings
# are instantiated per cluster size in bind_cluster_NxN.cpp; src/module.cpp
# registers everything in a fixed order.
#
# AARE_PYTHON_UNITY_BUILD combines the units of each group below into one. That
# trades parallelism and per-file rebuilds for less total compiler time, which
# is what counts on machines with few cores; pip builds and the packaging CI
# jobs turn it on. The build_with_docs CI job keeps it off, so every unit is
# also checked to compile on its own.
#
# Without NO_EXTRAS pybind11 links pybind11::lto, which forces full LTO on the
# extension regardless of the target's INTERPROCEDURAL_OPTIMIZATION property.
# LTO cannot reach aare_core (a plain static library) and the templated hot
# paths are already compiled at -O3 inside the extension, so it only adds a long
# single-threaded link step. With NO_EXTRAS the standard target property decides
# (off by default); the other extras are re-added below.
set(_aare_py_io
    src/bind_File.cpp
    src/bind_RawFile.cpp
    src/bind_RawSubFile.cpp
    src/bind_RawMasterFile.cpp
    src/bind_CtbRawFile.cpp
    src/bind_JungfrauDataFile.cpp
    src/bind_MultiThreadedFileReader.cpp
    src/bind_DataSetAggregation.cpp)
set(_aare_py_core src/bind_Defs.cpp src/bind_GapPixels.cpp
                  src/bind_PixelMap.cpp src/bind_testing.cpp)
set(_aare_py_hist src/bind_PixelHistogram.cpp
                  src/bind_PedestalTrackingPixelHistogram.cpp)
set(_aare_py_pedestal src/bind_Pedestal.cpp src/bind_FastPedestal.cpp
                      src/bind_calibration.cpp)
set(_aare_py_fit src/bind_Fit.cpp src/bind_Interpolator.cpp)
set(_aare_py_groups io core hist pedestal fit)

pybind11_add_module(
  _aare # name of the module
  NO_EXTRAS
  src/module.cpp
  # the cluster units take longest, listing them first schedules them first
  src/bind_cluster_3x3.cpp
  src/bind_cluster_9x9.cpp
  src/bind_cluster_7x7.cpp
  src/bind_cluster_5x5.cpp
  src/bind_cluster_2x2.cpp
  ${_aare_py_io}
  ${_aare_py_core}
  ${_aare_py_hist}
  ${_aare_py_pedestal}
  ${_aare_py_fit}
  src/bind_Eta.cpp
  src/bind_VarClusterFinder.cpp)

if(AARE_PYTHON_UNITY_BUILD)
  if(CMAKE_VERSION VERSION_LESS 3.18)
    message(
      WARNING
        "AARE_PYTHON_UNITY_BUILD needs CMake 3.18 or newer; compiling the python bindings as individual translation units"
    )
  else()
    set_target_properties(_aare PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE
                                                          GROUP)
    foreach(group IN LISTS _aare_py_groups)
      set_source_files_properties(${_aare_py_${group}} PROPERTIES UNITY_GROUP
                                                                  ${group})
    endforeach()
  endif()
endif()

target_link_libraries(_aare PRIVATE aare_core aare_compiler_flags)

# The pybind11_add_module extras that NO_EXTRAS skipped, LTO excepted
if(MSVC)
  target_link_libraries(_aare PRIVATE pybind11::windows_extras) # /bigobj, /MP
elseif(CMAKE_BUILD_TYPE)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" _aare_build_type)
  if(NOT _aare_build_type MATCHES "DEBUG|RELWITHDEBINFO|NONE")
    pybind11_strip(_aare) # strip symbols from optimized builds
  endif()
endif()

target_include_directories(
  _aare SYSTEM
  PRIVATE $<TARGET_PROPERTY:aare::Minuit2,INTERFACE_INCLUDE_DIRECTORIES>)

set_target_properties(
  _aare PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/aare
                   INTERPROCEDURAL_OPTIMIZATION FALSE)

# ---- CUDA module (separate .so) --------------------------------------------
if(AARE_CUDA)
  pybind11_add_module(_aare_cuda NO_EXTRAS src/cuda_bindings.cu)

  target_link_libraries(_aare_cuda PRIVATE aare_cuda aare_compiler_flags)

  target_include_directories(
    _aare_cuda SYSTEM
    PRIVATE $<TARGET_PROPERTY:Minuit2::Minuit2,INTERFACE_INCLUDE_DIRECTORIES>)

  set_target_properties(
    _aare_cuda
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/aare
               INTERPROCEDURAL_OPTIMIZATION FALSE
               CUDA_RESOLVE_DEVICE_SYMBOLS ON
               CUDA_SEPARABLE_COMPILATION ON)

  target_compile_options(
    _aare_cuda
    PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
            $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>
            $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fvisibility=hidden>
            $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-fPIC>)
endif()

# List of python files to be copied to the build directory
set(PYTHON_FILES
    aare/__init__.py
    aare/_version.py
    aare/CtbRawFile.py
    aare/ClusterFinder.py
    aare/ClusterVector.py
    aare/Cluster.py
    aare/FastPedestal.py
    aare/Pedestal.py
    aare/PixelHistogramCUDA.py
    aare/calibration.py
    aare/factory.py
    aare/experimental.py
    aare/RawFile.py
    aare/transform.py
    aare/ScanParameters.py
    aare/utils.py)

# Copy the python files to the build directory
foreach(FILE ${PYTHON_FILES})
  configure_file(${FILE} ${CMAKE_BINARY_DIR}/${FILE})
endforeach(FILE ${PYTHON_FILES})

# set_target_properties(_aare PROPERTIES LIBRARY_OUTPUT_DIRECTORY
# ${CMAKE_BINARY_DIR}/aare )

set(PYTHON_EXAMPLES examples/play.py examples/fits.py)

# Copy the python examples to the build directory
foreach(FILE ${PYTHON_EXAMPLES})
  configure_file(${FILE} ${CMAKE_BINARY_DIR}/${FILE})
  message(STATUS "Copying ${FILE} to ${CMAKE_BINARY_DIR}/${FILE}")
endforeach(FILE ${PYTHON_EXAMPLES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../VERSION
               ${CMAKE_BINARY_DIR}/aare/VERSION)
if(AARE_INSTALL_PYTHONEXT)
  set(AARE_PY_INSTALL_TARGETS _aare)
  if(AARE_CUDA)
    list(APPEND AARE_PY_INSTALL_TARGETS _aare_cuda)
  endif()

  install(
    TARGETS ${AARE_PY_INSTALL_TARGETS}
    EXPORT "${TARGETS_EXPORT_NAME}"
    LIBRARY DESTINATION aare COMPONENT python)

  install(
    FILES ${PYTHON_FILES}
    DESTINATION aare
    COMPONENT python)
  install(
    FILES ../VERSION
    DESTINATION aare
    COMPONENT python)
endif()
