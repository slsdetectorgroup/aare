/************************************************
 * @file Hdf5FileReader.test.cpp
 * @short test case for reading hdf5 files
 ***********************************************/

#include <filesystem>

#include <H5Cpp.h>

#include "aare/Hdf5FileReader.hpp"
#include "aare/NDArray.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

TEST_CASE("read hdf5 file", "[.hdf5file]") {

    // TODO generalize datasetpath
    std::string filename = "/home/mazzol_a/Documents/mythen3tools/beamline/"
                           "TDATA/ang1up_22keV_LaB60p3mm_48M_a_0320.h5";

    REQUIRE(std::filesystem::exists(filename));

    HDF5FileReader file_reader;

    file_reader.open_file(filename);

    auto dataset = file_reader.get_dataset("/entry/data/data");

    auto shape = dataset.get_shape();

    CHECK(shape[0] == 61440);

    auto type = dataset.get_datatype();

    const std::type_info *type_info = dataset.get_cpp_type();

    CHECK(*type_info == typeid(uint32_t));

    SECTION("read dataset into NDArray") {

        NDArray<uint32_t, 1> dataset_array =
            dataset.store_as_ndarray<uint32_t, 1>();

        CHECK(dataset_array(0) == 866);
        CHECK(dataset_array(61439) == 1436);
    }

    SECTION("read dataset into Frame") {
        Frame frame = dataset.store_as_frame();
        CHECK(*(reinterpret_cast<uint32_t *>(frame.pixel_ptr(0, 0))) == 866);
        CHECK(*(reinterpret_cast<uint32_t *>(frame.pixel_ptr(0, 61439))) ==
              1436);
    }
    SECTION("read subset of dataset") {
        Frame frame(1, 10, Dtype(typeid(uint32_t)));

        Subset subset{std::vector<hsize_t>{10}, std::vector<hsize_t>{10}};

        dataset.read_into_buffer(frame.data(), subset);

        CHECK(*(reinterpret_cast<uint32_t *>(frame.pixel_ptr(0, 0))) == 664);
        CHECK(*(reinterpret_cast<uint32_t *>(frame.pixel_ptr(0, 9))) == 654);
    }
    /*
    SECTION("read scalar") {
    }
    */
}

TEST_CASE("test datatypes", "[.hdf5file]") {

    auto [dtype, expected_type_info] = GENERATE(
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_INT), &typeid(int)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_INT8),
                        &typeid(int8_t)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_UINT16),
                        &typeid(uint16_t)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_INT16),
                        &typeid(int16_t)),
        std::make_tuple(H5::DataType(H5::PredType::STD_U32LE),
                        &typeid(uint32_t)),
        std::make_tuple(H5::DataType(H5::PredType::STD_I32LE),
                        &typeid(int32_t)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_INT32),
                        &typeid(int32_t)),
        std::make_tuple(H5::DataType(H5::PredType::IEEE_F64LE),
                        &typeid(double)),
        std::make_tuple(H5::DataType(H5::PredType::IEEE_F32LE), &typeid(float)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_FLOAT),
                        &typeid(float)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_DOUBLE),
                        &typeid(double)),
        std::make_tuple(H5::DataType(H5::PredType::NATIVE_CHAR),
                        &typeid(int8_t)));

    const std::type_info &type_info = deduce_cpp_type(dtype);

    CHECK(type_info == *expected_type_info);

    // TODO: handle bit swapping
    REQUIRE_THROWS(deduce_cpp_type(
        H5::DataType(H5::PredType::IEEE_F32BE))); // does not convert from big
                                                  // to little endian
}
