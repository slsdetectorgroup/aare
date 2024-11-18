.. code-block:: bash

    #build and install aare 
    git clone git@github.com:slsdetectorgroup/aare.git --branch=developer #or using http...
    mkdir build
    cd build

    #configure using cmake
    cmake ../aare -DCMAKE_INSTALL_PREFIX=/where/to/put/aare

    #build (replace 4 with the number of threads you want to use)
    make -j4 

    #install
    make install

    #Go to your project
    cd /your/project/source

    #Now configure your project
    mkdir build
    cd build
    cmake .. -DCMAKE_PREFIX_PATH=SOME_PATH