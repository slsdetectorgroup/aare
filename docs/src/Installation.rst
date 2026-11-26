****************
Installation
****************

.. attention ::

    - https://cliutils.gitlab.io/modern-cmake/README.html

conda/mamaba
~~~~~~~~~~~~~~~~~~

This is the recommended way to install aare. Using a package manager makes it easy to 
switch between versions and is (one of) the most convenient way to install up to date
dependencies on older distributions.

.. note ::

    aare is developing rapidly. Check for available releases by 
    using: **conda search aare -c slsdetectorgroup**

To install the latest version of aare using conda, run:

.. code-block:: bash 
    
    conda install aare -c slsdetectorgroup

To install a specific version of aare, run:

.. code-block:: bash

    # Install a specific version: 
    conda install aare=2024.11.11.dev0 -c slsdetectorgroup


cmake build (development install)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are working on aare or want to test out a version that doesn't yet have
a conda package. Build using cmake and then run from the build folder.

.. code-block:: bash

    git clone git@github.com:slsdetectorgroup/aare.git --branch=v1 #or using http...
    mkdir build
    cd build

    #configure using cmake
    cmake ../aare

    #build (replace 4 with the number of threads you want to use)
    make -j4 


    # add the build folder to your PYTHONPATH and then you should be able to
    # import aare in python

To build the python bindings and use aare in python compile with option ``-DAARE_PYTHON_BINDINGS=ON``. 

.. code-block:: bash

    #in build folder
    cmake ../aare -DAARE_PYTHON_BINDINGS=ON
    make -j4

Make sure to add the build folder to your PYTHONPATH and then you should be able to import aare in python.

.. code-block:: bash

    export PYTHONPATH=/path/to/aare/build:$PYTHONPATH

cmake build + install and use in your C++ project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. warning ::

    When building aare with default settings we also include fmt and nlohmann_json.
    Installation to a custom location is highly recommended.


.. note ::

    It is also possible to install aare with conda and then use in your C++ project.

.. include:: _install.rst


cmake options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For detailed options see the CMakeLists.txt file in the root directory of the project.

.. code-block:: bash

    # usage (or edit with ccmake .)
    cmake ../aare -DOPTION1=ON -DOPTION2=OFF


**AARE_SYSTEM_LIBRARIES "Use system libraries" OFF**

Look for fmt, nlohmann_json, pybind11 and Catch2 with ``find_package`` instead of building
them from source. Minuit2 and Google Benchmark are still built from source; turn them off
separately with ``AARE_FETCH_MINUIT2`` and ``AARE_FETCH_BENCHMARK``. Default option is off.

**AARE_FETCH_FROM_GITHUB "Download the sources of fetched dependencies from GitHub" OFF**

Download the dependencies that are built from source from GitHub instead of unpacking
the archives in ``libs/``. Default option is off.

Building without network access
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The source archives of fmt, nlohmann_json, Minuit2, pybind11, Catch2 and Google Benchmark
are part of the repository (``libs/``), and the default configuration builds them from
there. A CMake build of the library, the Python bindings, the tests and the benchmarks
therefore needs no network access. Two things still do:

- ``-DAARE_ZMQ=ON`` downloads libzmq, unless a system ZeroMQ is used with
  ``-DAARE_FETCH_ZMQ=OFF``.
- ``pip install .`` downloads the Python build requirements (scikit-build-core and
  numpy). Install them beforehand and pass ``--no-build-isolation`` to build offline.


**AARE_PYTHON_BINDINGS "Build python bindings" OFF**

Build the Python bindings. Default option is off. 

.. warning ::

    If you have a newer system Python compared to the one in your virtual environment,
    you might have to pass -DPython_FIND_VIRTUALENV=ONLY to cmake.

**AARE_PYTHON_UNITY_BUILD "Compile the python bindings as one translation unit per binding area" OFF**

Compile the Python bindings as one translation unit per binding area (file readers,
pedestals, clusters of one size, ...) instead of one per bound class. The build then
needs less total compiler time but has fewer units to compile in parallel, which is
faster on machines with few cores. Default option is off; ``pip install`` and the packaging
CI jobs turn it on.

**AARE_TESTS "Build tests" OFF**

Build unit tests. Default option is off.

**AARE_EXAMPLES "Build examples" OFF**

**AARE_DOCS "Build documentation" OFF**

Build documentation. Needs doxygen, sphinx and breathe. Default option is off.
Requires a separate make docs.

**AARE_VERBOSE "Verbose output" OFF**

**AARE_CUSTOM_ASSERT "Use custom assert" OFF**

Enable custom assert macro to check for errors. Default option is off.