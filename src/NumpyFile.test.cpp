#include "aare/NumpyFile.hpp"
#include "aare/NDArray.hpp"

#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

using aare::Dtype;
using aare::NumpyFile;
TEST_CASE("Read a 1D numpy file with int32 data type", "[.with-data]") {

    auto fpath = test_data_path() / "numpy" / "test_1d_int32.npy";
    REQUIRE(std::filesystem::exists(fpath));

    NumpyFile f(fpath);

    // we know the file contains 10 elements of np.int32 containing values 0-9
    REQUIRE(f.dtype() == Dtype::INT32);
    REQUIRE(f.shape() == std::vector<size_t>{10});

    // use the load function to read the full file into a NDArray
    auto data = f.load<int32_t, 1>();
    for (int32_t i = 0; i < 10; i++) {
        REQUIRE(data(i) == i);
    }
}

TEST_CASE("Read a 3D numpy file with np.double data type", "[.with-data]") {

    auto fpath = test_data_path() / "numpy" / "test_3d_double.npy";
    REQUIRE(std::filesystem::exists(fpath));

    NumpyFile f(fpath);

    // we know the file contains 10 elements of np.int32 containing values 0-9
    REQUIRE(f.dtype() == Dtype::DOUBLE);
    REQUIRE(f.shape() == std::vector<size_t>{3, 2, 5});

    // use the load function to read the full file into a NDArray
    // numpy code to generate the array
    //  arr2[0,0,0] = 1.0
    //  arr2[0,0,1] = 2.0
    //  arr2[0,1,0] = 72.0
    //  arr2[2,0,4] = 63.0

    auto data = f.load<double, 3>();
    REQUIRE(data(0, 0, 0) == 1.0);
    REQUIRE(data(0, 0, 1) == 2.0);
    REQUIRE(data(0, 1, 0) == 72.0);
    REQUIRE(data(2, 0, 4) == 63.0);
}