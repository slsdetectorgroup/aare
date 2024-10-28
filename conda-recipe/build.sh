

mkdir build
mkdir install
cd build
cmake .. \
      -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
      -DCMAKE_INSTALL_PREFIX=install \
      -DAARE_SYSTEM_LIBRARIES=ON \
      -DAARE_TESTS=ON \
      -DAARE_PYTHON_BINDINGS=ON \
      -DCMAKE_BUILD_TYPE=Release \

     
NCORES=$(getconf _NPROCESSORS_ONLN)
echo "Building using: ${NCORES} cores"
cmake --build . -- -j${NCORES}
cmake --build . --target install

# CTEST_OUTPUT_ON_FAILURE=1 ctest -j 1