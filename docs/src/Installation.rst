Installation
===============

conda/mamaba
~~~~~~~~~~~~~~~~

.. note ::

    aare is developing rapidly. Check for the latest release by 
    using: **conda search aare -c slsdetectorgroup**


.. code-block:: bash

    # Install a specific version: 
    conda install aare=2024.11.11.dev0 -c slsdetectorgroup


cmake (development install)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    git clone git@github.com:slsdetectorgroup/aare.git --branch=v1 #or using http...
    mkdir build
    cd build

    #configure using cmake
    cmake ../aare

    #build (replace 4 with the number of threads you want to use)
    make -j4 


    # add the build folder to your PYTHONPATH

cmake install and use in your C++ project
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    #build and install aare 
    git clone git@github.com:slsdetectorgroup/aare.git --branch=v1 #or using http...
    mkdir build
    cd build

    #configure using cmake
    cmake ../aare -DCMAKE_INSTALL_PREFIX=/where/to/put/aare

    #build (replace 4 with the number of threads you want to use)
    make -j4 

    #install
    make install


    #Now configure your project
    cmake .. -DCMAKE_PREFIX_PATH=SOME_PATH


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