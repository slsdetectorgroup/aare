C++ examples
============

The examples are standalone programs in the ``examples/`` directory of the
repository. They are built with ``-DAARE_EXAMPLES=ON`` and run as part of the
tests, so the code shown here compiles and runs with the current version of
the library.

.. code-block:: bash

    cmake -S . -B build -DAARE_EXAMPLES=ON
    cmake --build build -j4
    ctest --test-dir build -R example

Fit a Gaussian
--------------

Fit a Gaussian peak to a one-dimensional histogram with
``aare::fit_pixel``.

.. literalinclude:: ../../examples/fit_gaussian.cpp
    :language: cpp
