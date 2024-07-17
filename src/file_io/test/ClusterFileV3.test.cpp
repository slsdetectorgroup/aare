#include "aare/file_io/ClusterFileV3.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("ClusterFileV3::Field::to_json") {
    v3::Field f;
    f.label = "TEST";
    f.dtype = Dtype::INT32;
    f.is_array = f.FIXED_LENGTH_ARRAY;
    f.array_size = 10;
    REQUIRE(f.to_json() ==
            "{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10}");
}

TEST_CASE("ClusterFileV3::Field::to_json2") {
    v3::Field f;
    f.label = "123abc";
    f.dtype = Dtype::DOUBLE;
    f.is_array = f.VARIABLE_LENGTH_ARRAY;
    f.array_size = 0;
    REQUIRE(f.to_json() ==
            "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}");
}

TEST_CASE("ClusterFileV3::Field::from_json") {
    v3::Field f;
    f.from_json("{\"label\": \"abc\", \"dtype\": \"<u1\", \"is_array\": 1, \"array_size\": 0}");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}
TEST_CASE("ClusterFileV3::Field::from_json2") {
    v3::Field f;
    f.from_json("{\n\n\n\n\n\t\"label\": \n\n\t\"abc\", \t\n\n\"dtype\": \"<u1\", \"is_array\": 1, "
                "\n\"array_size\":\n "
                "0}\n\n\n\n\t");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}

TEST_CASE("ClusterFileV3::Header::from_json test empty string") {
    std::string json = "{         }";
    v3::Header h;
    h.from_json(json);
    REQUIRE(h.version == h.CURRENT_VERSION);
    REQUIRE(h.n_records == 0);
    REQUIRE(h.metadata.size() == 0);
    REQUIRE(h.header_fields.size() == 0);
    REQUIRE(h.data_fields.size() == 0);
}
TEST_CASE("ClusterFileV3::Header::from_json test with \\n and \\t") {
    std::string json = "{\n\t\"version\":   \"0.1\",\n\n\t  \"n_records\":     \"100\"    \n}";
    v3::Header h;
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
    v3::Header h;
    h.from_json(json);
    REQUIRE(h.metadata ==
            std::map<std::string, std::string>{{"key1", "value1"}, {"key2", "value2"}});
}

TEST_CASE("ClusterFileV3::Header::from_json data field with \\n and \\t") {
    std::string json =
        "{ \n\n\"data_fields\": [\n {\n \"label\":\n\t \"XXXABC\", \n\t\"dtype\":\t\n \"<i4\", "
        "\"is_array\": 1, \"array_size\": 10\n } \n ]\n }\n";
    v3::Header h;
    h.from_json(json);
    REQUIRE(h.data_fields.size() == 1);
    REQUIRE(h.data_fields[0].label == "XXXABC");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == v3::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
}

TEST_CASE("ClusterFileV3::Header::from_json") {
    std::string json =
        "{"
        "\"version\": \"1.2\","
        "\"n_records\": \"100\","
        "\"metadata\": {\"key1\": \"value1\", \"key2\": \"value2\"},"
        "\"header_fields\": "
        "[{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10},"
        "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}],"
        "\"data_fields\": "
        "[{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10},"
        "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}]"

        "}";
    v3::Header h;
    h.from_json(json);
    REQUIRE(h.version == "1.2");
    REQUIRE(h.n_records == 100);
    REQUIRE(h.metadata.size() == 2);
    REQUIRE(h.metadata.at("key1") == "value1");
    REQUIRE(h.metadata.at("key2") == "value2");
    REQUIRE(h.header_fields.size() == 2);
    REQUIRE(h.header_fields[0].label == "TEST");
    REQUIRE(h.header_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.header_fields[0].is_array == v3::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.header_fields[0].array_size == 10);
    REQUIRE(h.header_fields[1].label == "123abc");
    REQUIRE(h.header_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.header_fields[1].is_array == v3::Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.header_fields[1].array_size == 0);
    REQUIRE(h.data_fields.size() == 2);
    REQUIRE(h.data_fields[0].label == "TEST");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == v3::Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
    REQUIRE(h.data_fields[1].label == "123abc");
    REQUIRE(h.data_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.data_fields[1].is_array == v3::Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.data_fields[1].array_size == 0);
}

TEST_CASE("write file with fixed length data structure") {
    v3::Header header;
    header.metadata["nSigma"] = "77";
    header.version = "0.7";
    header.header_fields = v3::ClusterHeader::get_fields();
    header.data_fields = v3::ClusterData<9>::get_fields();
    v3::ClusterFile<v3::ClusterHeader, v3::ClusterData<9>> file("/tmp/test_fixed_112233.clust2",
                                                                "w", header);

    for (int j = 0; j < 100; j++) {
        v3::ClusterHeader clust_header;
        std::vector<v3::ClusterData<9>> clust_data;

        clust_header.frame_number = j;
        clust_header.n_clusters = 500 + j;
        for (int i = 0; i < clust_header.n_clusters; i++) {
            v3::ClusterData c;
            c.m_x = i;
            c.m_y = j;
            c.m_data = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            clust_data.push_back(c);
        }
        file.write(clust_header, clust_data);
    }
    file.close();

    v3::ClusterFile<v3::ClusterHeader, v3::ClusterData<9>> file2("/tmp/test_fixed_112233.clust2",
                                                                 "r");
    auto header_ = file2.header();
    REQUIRE(header_.metadata["nSigma"] == "77");
    REQUIRE(header_.version == "0.7");
    REQUIRE(header_.n_records == 100);
    REQUIRE((header_.header_fields == v3::ClusterHeader::get_fields()));
    REQUIRE((header_.data_fields == v3::ClusterData<9>::get_fields()));

    for (int j = 0; j < 100; j++) {
        auto [header2, clusters2] = file2.read();
        REQUIRE(header2.frame_number == j);
        REQUIRE(header2.n_clusters == 500 + j);
        for (int i = 0; i < header2.n_clusters; i++) {
            REQUIRE(clusters2[i].m_x == i);
            REQUIRE(clusters2[i].m_y == j);
            REQUIRE(clusters2[i].m_data == std::array<int32_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9});
        }
    }
    REQUIRE_THROWS_AS(file2.read(), std::invalid_argument);
}

TEST_CASE("write/read clust2 file with variable data") {
    v3::Header header;
    header.metadata["hello world"] = "testing11";

    header.header_fields = v3::ClusterHeader::get_fields();
    header.data_fields = v3::ClusterDataVlen::get_fields();

    v3::ClusterFile<v3::ClusterHeader, v3::ClusterDataVlen> file("/tmp/test_vlen_332211.clust2",
                                                                 "w", header);

    v3::ClusterHeader cluster_header;
    cluster_header.frame_number = 812304;
    cluster_header.n_clusters = 20;
    std::vector<v3::ClusterDataVlen> clusters;
    for (auto i = 0; i < 20; i++) {
        v3::ClusterDataVlen cluster;
        cluster.x = std::vector<int16_t>(i + 1, i);
        cluster.y = std::vector<int16_t>(i + 1, 10 + i);
        cluster.energy = std::vector<int32_t>(i + 1, 20.0 + i);
        clusters.emplace_back(cluster);
    }
    file.write(cluster_header, clusters);
    file.close();

    v3::ClusterFile<v3::ClusterHeader, v3::ClusterDataVlen> file2("/tmp/test_vlen_332211.clust2",
                                                                  "r");

    auto file_header = file2.header();
    REQUIRE(file_header.metadata["hello world"] == "testing11");

    auto [header2, clusters2] = file2.read();
    REQUIRE(header2.frame_number == 812304);
    REQUIRE(header2.n_clusters == 20);
    for (auto i = 0; i < 20; i++) {
        REQUIRE(clusters2[i].x == std::vector<int16_t>(i + 1, i));
        REQUIRE(clusters2[i].y == std::vector<int16_t>(i + 1, 10 + i));
        REQUIRE(clusters2[i].energy == std::vector<int32_t>(i + 1, 20.0 + i));
    }
    REQUIRE_THROWS_AS(file2.read(), std::invalid_argument);
}