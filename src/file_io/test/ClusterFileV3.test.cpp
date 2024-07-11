#include "aare/file_io/ClusterFileV3.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("ClusterFileV3::Field::to_json") {
    v3::ClusterFile::Field f;
    f.label = "TEST";
    f.dtype = Dtype::INT32;
    f.is_array = f.FIXED_LENGTH_ARRAY;
    f.array_size = 10;
    REQUIRE(f.to_json() == "{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10}");
}

TEST_CASE("ClusterFileV3::Field::to_json2") {
    v3::ClusterFile::Field f;
    f.label = "123abc";
    f.dtype = Dtype::DOUBLE;
    f.is_array = f.VARIABLE_LENGTH_ARRAY;
    f.array_size = 0;
    REQUIRE(f.to_json() == "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}");
}

TEST_CASE("ClusterFileV3::Field::from_json") {
    v3::ClusterFile::Field f;
    f.from_json("{\"label\": \"abc\", \"dtype\": \"<u1\", \"is_array\": 1, \"array_size\": 0}");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}
TEST_CASE("ClusterFileV3::Field::from_json2") {
    v3::ClusterFile::Field f;
    f.from_json("{\n\n\n\n\n\t\"label\": \n\n\t\"abc\", \t\n\n\"dtype\": \"<u1\", \"is_array\": 1, \n\"array_size\":\n "
                "0}\n\n\n\n\t");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}

TEST_CASE("ClusterFileV3::Header::from_json test empty string") {
    std::string json = "{         }";
    v3::ClusterFile::Header h;
    h.from_json(json);
    REQUIRE(h.version == h.CURRENT_VERSION);
    REQUIRE(h.n_records == 0);
    REQUIRE(h.metadata.size() == 0);
    REQUIRE(h.header_fields.size() == 0);
    REQUIRE(h.data_fields.size() == 0);
}
TEST_CASE("ClusterFileV3::Header::from_json test with \\n and \\t") {
    std::string json = "{\n\t\"version\":   \"0.1\",\n\n\t  \"n_records\":     100    \n}";
    v3::ClusterFile::Header h;
    h.from_json(json);
    REQUIRE(h.version == "0.1");
    REQUIRE(h.n_records == 100);
    REQUIRE(h.metadata.size() == 0);
    REQUIRE(h.header_fields.size() == 0);
    REQUIRE(h.data_fields.size() == 0);
}
TEST_CASE("ClusterFileV3::Header::from_json metadata with \\n and \\t") {
    std::string json = "{\n\t\n\t\n\t \"metadata\":\n\t\n {\n\t\"key1\":\n \"value1\", "
                       "\t\n\n\"key2\":\t\n\n \"value2\"\t\n\t\n}\n }";
    v3::ClusterFile::Header h;
    h.from_json(json);
    REQUIRE(h.metadata == std::map<std::string, std::string>{{"key1", "value1"}, {"key2", "value2"}});
}

TEST_CASE("ClusterFileV3::Header::from_json data field with \\n and \\t") {
    std::string json = "{ \n\n\"data_fields\": [\n {\n \"label\":\n\t \"XXXABC\", \n\t\"dtype\":\t\n \"<i4\", "
                       "\"is_array\": 1, \"array_size\": 10\n } \n ]\n }\n";
    v3::ClusterFile::Header h;
    h.from_json(json);
    REQUIRE(h.data_fields.size() == 1);
    REQUIRE(h.data_fields[0].label == "XXXABC");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == v3::ClusterFile::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
}

TEST_CASE("ClusterFileV3::Header::from_json") {
    std::string json = "{"
                       "\"version\": \"1.2\","
                       "\"n_records\": 100,"
                       "\"metadata\": {\"key1\": \"value1\", \"key2\": \"value2\"},"
                       "\"header_fields\": "
                       "[{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10},"
                       "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}],"
                       "\"data_fields\": "
                       "[{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10},"
                       "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}]"

                       "}";
    v3::ClusterFile::Header h;
    h.from_json(json);
    REQUIRE(h.version == "1.2");
    REQUIRE(h.n_records == 100);
    REQUIRE(h.metadata.size() == 2);
    REQUIRE(h.metadata.at("key1") == "value1");
    REQUIRE(h.metadata.at("key2") == "value2");
    REQUIRE(h.header_fields.size() == 2);
    REQUIRE(h.header_fields[0].label == "TEST");
    REQUIRE(h.header_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.header_fields[0].is_array == v3::ClusterFile::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.header_fields[0].array_size == 10);
    REQUIRE(h.header_fields[1].label == "123abc");
    REQUIRE(h.header_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.header_fields[1].is_array == v3::ClusterFile::Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.header_fields[1].array_size == 0);
    REQUIRE(h.data_fields.size() == 2);
    REQUIRE(h.data_fields[0].label == "TEST");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == v3::ClusterFile::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
    REQUIRE(h.data_fields[1].label == "123abc");
    REQUIRE(h.data_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.data_fields[1].is_array == v3::ClusterFile::Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.data_fields[1].array_size == 0);
}