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

    aare is developing rapidly. Check for the latest release by 
    using: **conda search aare -c slsdetectorgroup**


.. code-block:: bash

    # Install a specific version: 
    conda install aare=2024.11.11.dev0 -c slsdetectorgroup


cmake build (development install)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are working on aare or want to test our a version that doesn't yet have
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

Use system libraries instead of using FetchContent to pull in dependencies. Default option is off.


**AARE_PYTHON_BINDINGS "Build python bindings" ON**

Build the Python bindings. Default option is on. 

.. warning ::

    If you have a newer system Python compared to the one in your virtual environment,
    you might have to pass -DPython_FIND_VIRTUALENV=ONLY to cmake.

**AARE_TESTS "Build tests" OFF**

Build unit tests. Default option is off.

**AARE_EXAMPLES "Build examples" OFF**

**AARE_DOCS "Build documentation" OFF**

Build documentation. Needs doxygen, sphinx and breathe. Default option is off.
Requires a separate make docs.

**AARE_VERBOSE "Verbose output" OFF**

**AARE_CUSTOM_ASSERT "Use custom assert" OFF**

Enable custom assert macro to check for errors. Default option is off.