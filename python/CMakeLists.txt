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
    GIT_TAG v3.0.3)
  FetchContent_MakeAvailable(pybind11)
else()
  find_package(pybind11 3.0.3 REQUIRED)
endif()

# ---- Main CPU module --------------------------------------------------------
# module.cpp is the only source for the main module. When AARE_CUDA=ON, the CUDA
# bindings live in a *separate* Python extension (_aare_cuda.so) loaded
# independently at runtime. This isolates the nvcc-compiled translation unit
# into its own ELF image so pybind11's type registry cannot be corrupted by
# weak-symbol collisions between gcc-emitted and nvcc-emitted template
# instantiations.
pybind11_add_module(_aare NO_EXTRAS src/module.cpp)

target_link_libraries(_aare PRIVATE aare_core aare_compiler_flags)
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
    aare/calibration.py
    aare/func.py
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
