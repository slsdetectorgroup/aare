****************
Tests
****************

We test the code both from the C++ and Python API. By default only tests that does not require image data is run. 

C++
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    mkdir build
    cd build
    cmake .. -DAARE_TESTS=ON
    make -j 4

    export AARE_TEST_DATA=/path/to/test/data
    ./run_test [.files] #or using ctest, [.files] is the option to include tests needing data



Python
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    #From the root dir of the library
    python -m pytest python/tests --files # passing --files will run the tests needing data



Getting the test data
~~~~~~~~~~~~~~~~~~~~~~~~

.. attention ::

    The tests needing the test data are not run by default. To make the data available, you need to set the environment variable
    AARE_TEST_DATA to the path of the test data directory. Then pass either [.files] for the C++ tests or --files for Python

The image files needed for the test are large and are not included in the repository. They are stored
using GIT LFS in a separate repository. To get the test data, you need to clone the repository.
To do this, you need to have GIT LFS installed. You can find instructions on how to install it here: https://git-lfs.github.com/
Once you have GIT LFS installed, you can clone the repository like any normal repo using:

.. code-block:: bash
    
    git clone https://gitea.psi.ch/detectors/aare-test-data.git
