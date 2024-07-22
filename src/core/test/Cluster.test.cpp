#include "aare/core/Cluster.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Field::to_json") {
    Field f;
    f.label = "TEST";
    f.dtype = Dtype::INT32;
    f.is_array = f.FIXED_LENGTH_ARRAY;
    f.array_size = 10;
    REQUIRE(f.to_json() ==
            "{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10}");
}

TEST_CASE("Field::to_json2") {
    Field f;
    f.label = "123abc";
    f.dtype = Dtype::DOUBLE;
    f.is_array = f.VARIABLE_LENGTH_ARRAY;
    f.array_size = 0;
    REQUIRE(f.to_json() ==
            "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}");
}

TEST_CASE("Field::from_json") {
    Field f;
    f.from_json("{\"label\": \"abc\", \"dtype\": \"<u1\", \"is_array\": 1, \"array_size\": 0}");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}
TEST_CASE("Field::from_json2") {
    Field f;
    f.from_json("{\n\n\n\n\n\t\"label\": \n\n\t\"abc\", \t\n\n\"dtype\": \"<u1\", \"is_array\": 1, "
                "\n\"array_size\":\n "
                "0}\n\n\n\n\t");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}

TEST_CASE("ClusterHeader") {
    ClusterHeader ch;
    ch.frame_number = 100;
    ch.n_clusters = 10;
    REQUIRE(ch.frame_number == 100);
    REQUIRE(ch.n_clusters == 10);
    REQUIRE_NOTHROW(ch.to_string());

    auto fields = ClusterHeader::get_fields();
    REQUIRE(fields.size() == 2);
    REQUIRE(fields[0].label == "frame_number");
    REQUIRE(fields[0].dtype == Dtype::INT32);
    REQUIRE(fields[0].is_array == Field::NOT_ARRAY);
    REQUIRE(fields[0].array_size == 0);
    REQUIRE(fields[1].label == "n_clusters");
    REQUIRE(fields[1].dtype == Dtype::INT32);
    REQUIRE(fields[1].is_array == Field::NOT_ARRAY);
    REQUIRE(fields[1].array_size == 0);

    REQUIRE(ch.data_count() == 10);
    REQUIRE(ch.size() == 2 * sizeof(int32_t));
    REQUIRE(ch.size() == sizeof(ch));
    REQUIRE(ch.has_data());
    REQUIRE(ch.data() != nullptr);
    REQUIRE(ch.to_string() != "");
}
TEST_CASE("test cluster header set/get") {
    ClusterHeader ch2;
    std::array<int32_t, 2> data = {11, 22};
    ch2.set(reinterpret_cast<std::byte *>(data.data()));
    REQUIRE(ch2.frame_number == 11);
    REQUIRE(ch2.n_clusters == 22);

    std::array<int32_t, 2> data2;
    ch2.get(reinterpret_cast<std::byte *>(data2.data()));
    REQUIRE(data2[0] == 11);
    REQUIRE(data2[1] == 22);
}

TEST_CASE("test templated ClusterData") {
    tClusterData<int8_t, 100> cd(1, 2, std::array<int8_t, 100>{});
    REQUIRE(cd.size() == 100 * sizeof(int8_t) + 2 * sizeof(int16_t));
    REQUIRE(cd.size() == sizeof(cd));
    REQUIRE(cd.has_data());
    REQUIRE(cd.data() != nullptr);
    REQUIRE(cd.to_string() != "");
    REQUIRE(cd.x == 1);
    REQUIRE(cd.y == 2);
    REQUIRE(cd.array == std::array<int8_t, 100>{});

    auto fields = tClusterData<int8_t, 100>::get_fields();
    REQUIRE(fields.size() == 3);
    REQUIRE(fields[0].label == "x");
    REQUIRE(fields[0].dtype == Dtype::INT16);
    REQUIRE(fields[0].is_array == Field::NOT_ARRAY);
    REQUIRE(fields[0].array_size == 0);
    REQUIRE(fields[1].label == "y");
    REQUIRE(fields[1].dtype == Dtype::INT16);
    REQUIRE(fields[1].is_array == Field::NOT_ARRAY);
    REQUIRE(fields[1].array_size == 0);
    REQUIRE(fields[2].label == "data");
    REQUIRE(fields[2].dtype == Dtype::INT8);
    REQUIRE(fields[2].is_array == Field::FIXED_LENGTH_ARRAY);
    REQUIRE(fields[2].array_size == 100);

    REQUIRE_THROWS(cd.set_fields(fields));

    std::array<int16_t, 2> data1 = {11, 22};
    std::array<int8_t, 100> data2 = {-1, 2, 3, 4, -5};
    std::array<std::byte, sizeof(data1) + sizeof(data2)> data;
    std::memcpy(data.data(), data1.data(), sizeof(data1));
    std::memcpy(data.data() + sizeof(data1), data2.data(), sizeof(data2));
    cd.set(reinterpret_cast<std::byte *>(data.data()));
    REQUIRE(cd.x == 11);
    REQUIRE(cd.y == 22);
    REQUIRE(cd.array == data2);

    cd.x = 33;
    cd.y = 44;
    cd.array = {1, 2, 3, 4, 5};
    std::array<std::byte, sizeof(data1) + sizeof(data2)> data3;
    cd.get(reinterpret_cast<std::byte *>(data3.data()));
    REQUIRE(std::memcmp(data3.data(), &cd.x, sizeof(cd.x)) == 0);
    REQUIRE(std::memcmp(data3.data() + sizeof(cd.x), &cd.y, sizeof(cd.y)) == 0);
    REQUIRE(std::memcmp(data3.data() + sizeof(cd.x) + sizeof(cd.y), cd.array.data(),
                        cd.array.size()) == 0);
}
TEST_CASE("test dynamic ClusterData") {
    DynamicClusterData cd(9, Dtype::INT32);
    REQUIRE(cd.size() == 9 * sizeof(int32_t) + 2 * sizeof(int16_t));
    REQUIRE(cd.has_data() == false);
    REQUIRE(cd.count == 9);
    REQUIRE(cd.dtype == Dtype::INT32);

    auto fields = cd.get_fields();
    REQUIRE((fields == tClusterData<int32_t, 9>::get_fields()));
    REQUIRE((DynamicClusterData(1, Dtype::DOUBLE).get_fields() ==
             tClusterData<double, 1>::get_fields()));
    REQUIRE((DynamicClusterData(100, Dtype::UINT8).get_fields() ==
             tClusterData<uint8_t, 100>::get_fields()));

    DynamicClusterData cd2(tClusterData<int16_t, 4>::get_fields());
    REQUIRE(cd2.count == 4);
    REQUIRE(cd2.dtype == Dtype::INT16);
    REQUIRE(cd2.size() == 4 * sizeof(int16_t) + 2 * sizeof(int16_t));
    REQUIRE(cd2.has_data() == false);
    REQUIRE((cd2.get_fields() == tClusterData<int16_t, 4>::get_fields()));
}

TEST_CASE("test dynamic clusterData set/get") {
    DynamicClusterData cd;
    cd.set_fields(tClusterData<int16_t, 4>::get_fields());
    std::array<int16_t, 6> data = {20, 30, 1, 2, -3, 4};
    cd.set(reinterpret_cast<std::byte *>(data.data()));
    REQUIRE(cd.x == 20);
    REQUIRE(cd.y == 30);
    REQUIRE(cd.get_array<int16_t>(0) == 1);
    REQUIRE(cd.get_array<int16_t>(1) == 2);
    REQUIRE(cd.get_array<int16_t>(2) == -3);
    REQUIRE(cd.get_array<int16_t>(3) == 4);
    REQUIRE(cd.get_array<int16_t,4>() == std::array<int16_t, 4>{1, 2, -3, 4});

    cd.x = 40;
    cd.y = 50;
    cd.set_array<int16_t>(0, 10);
    cd.get(reinterpret_cast<std::byte *>(data.data()));
    REQUIRE(cd.x == 40);
    REQUIRE(cd.y == 50);
    REQUIRE(data[0] == 40);
    REQUIRE(data[1] == 50);
    REQUIRE(data[2] == 10);
    REQUIRE(data[3] == 2);
    REQUIRE(data[4] == -3);
    REQUIRE(data[5] == 4);


}
