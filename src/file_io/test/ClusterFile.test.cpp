#include "aare/file_io/ClusterFile.hpp"
#include "aare/file_io/ClusterFileImplementation.hpp"
#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("ClusterFileV3::Field::to_json") {
    Field f;
    f.label = "TEST";
    f.dtype = Dtype::INT32;
    f.is_array = f.FIXED_LENGTH_ARRAY;
    f.array_size = 10;
    REQUIRE(f.to_json() ==
            "{\"label\": \"TEST\", \"dtype\": \"<i4\", \"is_array\": 1, \"array_size\": 10}");
}

TEST_CASE("ClusterFileV3::Field::to_json2") {
    Field f;
    f.label = "123abc";
    f.dtype = Dtype::DOUBLE;
    f.is_array = f.VARIABLE_LENGTH_ARRAY;
    f.array_size = 0;
    REQUIRE(f.to_json() ==
            "{\"label\": \"123abc\", \"dtype\": \"f8\", \"is_array\": 2, \"array_size\": 0}");
}

TEST_CASE("ClusterFileV3::Field::from_json") {
    Field f;
    f.from_json("{\"label\": \"abc\", \"dtype\": \"<u1\", \"is_array\": 1, \"array_size\": 0}");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}
TEST_CASE("ClusterFileV3::Field::from_json2") {
    Field f;
    f.from_json("{\n\n\n\n\n\t\"label\": \n\n\t\"abc\", \t\n\n\"dtype\": \"<u1\", \"is_array\": 1, "
                "\n\"array_size\":\n "
                "0}\n\n\n\n\t");
    REQUIRE(f.label == "abc");
    REQUIRE(f.dtype == Dtype::UINT8);
    REQUIRE(f.is_array == f.FIXED_LENGTH_ARRAY);
    REQUIRE(f.array_size == 0);
}

TEST_CASE("ClusterFileV3::ClusterFileHeader::from_json test empty string") {
    std::string json = "{         }";
    ClusterFileHeader h;
    h.from_json(json);
    REQUIRE(h.version == h.CURRENT_VERSION);
    REQUIRE(h.n_records == 0);
    REQUIRE(h.metadata.size() == 0);
    REQUIRE(h.header_fields.size() == 0);
    REQUIRE(h.data_fields.size() == 0);
}
TEST_CASE("ClusterFileV3::ClusterFileHeader::from_json test with \\n and \\t") {
    std::string json = "{\n\t\"version\":   \"0.1\",\n\n\t  \"n_records\":     \"100\"    \n}";
    ClusterFileHeader h;
    h.from_json(json);
    REQUIRE(h.version == "0.1");
    REQUIRE(h.n_records == 100);
    REQUIRE(h.metadata.size() == 0);
    REQUIRE(h.header_fields.size() == 0);
    REQUIRE(h.data_fields.size() == 0);
}
TEST_CASE("ClusterFileV3::ClusterFileHeader::from_json metadata with \\n and \\t") {
    std::string json = "{\n\t\n\t\n\t \"metadata\":\n\t\n {\n\t\"key1\":\n \"value1\", "
                       "\t\n\n\"key2\":\t\n\n \"value2\"\t\n\t\n}\n }";
    ClusterFileHeader h;
    h.from_json(json);
    REQUIRE(h.metadata ==
            std::map<std::string, std::string>{{"key1", "value1"}, {"key2", "value2"}});
}

TEST_CASE("ClusterFileV3::ClusterFileHeader::from_json data field with \\n and \\t") {
    std::string json =
        "{ \n\n\"data_fields\": [\n {\n \"label\":\n\t \"XXXABC\", \n\t\"dtype\":\t\n \"<i4\", "
        "\"is_array\": 1, \"array_size\": 10\n } \n ]\n }\n";
    ClusterFileHeader h;
    h.from_json(json);
    REQUIRE(h.data_fields.size() == 1);
    REQUIRE(h.data_fields[0].label == "XXXABC");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
}

TEST_CASE("ClusterFileV3::ClusterFileHeader::from_json") {
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
    ClusterFileHeader h;
    h.from_json(json);
    REQUIRE(h.version == "1.2");
    REQUIRE(h.n_records == 100);
    REQUIRE(h.metadata.size() == 2);
    REQUIRE(h.metadata.at("key1") == "value1");
    REQUIRE(h.metadata.at("key2") == "value2");
    REQUIRE(h.header_fields.size() == 2);
    REQUIRE(h.header_fields[0].label == "TEST");
    REQUIRE(h.header_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.header_fields[0].is_array == Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.header_fields[0].array_size == 10);
    REQUIRE(h.header_fields[1].label == "123abc");
    REQUIRE(h.header_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.header_fields[1].is_array == Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.header_fields[1].array_size == 0);
    REQUIRE(h.data_fields.size() == 2);
    REQUIRE(h.data_fields[0].label == "TEST");
    REQUIRE(h.data_fields[0].dtype == Dtype::INT32);
    REQUIRE(h.data_fields[0].is_array == Field::FIXED_LENGTH_ARRAY);
    REQUIRE(h.data_fields[0].array_size == 10);
    REQUIRE(h.data_fields[1].label == "123abc");
    REQUIRE(h.data_fields[1].dtype == Dtype::DOUBLE);
    REQUIRE(h.data_fields[1].is_array == Field::VARIABLE_LENGTH_ARRAY);
    REQUIRE(h.data_fields[1].array_size == 0);
}

TEST_CASE("write file with fixed length data structure") {
    ClusterFileHeader header;
    header.metadata["nSigma"] = "77";
    header.version = "0.7";
    header.header_fields = ClusterHeader::get_fields();
    header.data_fields = ClusterData<int32_t,9>::get_fields();
    ClusterFile<ClusterHeader, ClusterData<int32_t,9>> file("/tmp/test_fixed_112233.clust2",
                                                                "w", header);

    for (int j = 0; j < 100; j++) {
        ClusterHeader clust_header;
        std::vector<ClusterData<int32_t,9>> clust_data;

        clust_header.frame_number = j;
        clust_header.n_clusters = 500 + j;
        for (int i = 0; i < clust_header.n_clusters; i++) {
            ClusterData c;
            c.x = i;
            c.y = j;
            c.array = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            clust_data.push_back(c);
        }
        file.write(clust_header, clust_data);
    }
    file.close();

    ClusterFile<ClusterHeader, ClusterData<int32_t,9>> file2("/tmp/test_fixed_112233.clust2",
                                                                 "r");
    auto header_ = file2.header();
    REQUIRE(header_.metadata["nSigma"] == "77");
    REQUIRE(header_.version == "0.7");
    REQUIRE(header_.n_records == 100);
    REQUIRE((header_.header_fields == ClusterHeader::get_fields()));
    REQUIRE((header_.data_fields == ClusterData<int32_t,9>::get_fields()));

    for (int j = 0; j < 100; j++) {
        auto [header2, clusters2] = file2.read();
        REQUIRE(header2.frame_number == j);
        REQUIRE(header2.n_clusters == 500 + j);
        for (int i = 0; i < header2.n_clusters; i++) {
            REQUIRE(clusters2[i].x == i);
            REQUIRE(clusters2[i].y == j);
            REQUIRE(clusters2[i].array == std::array<int32_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9});
        }
    }
    REQUIRE_THROWS_AS(file2.read(), std::invalid_argument);
}

TEST_CASE("write/read clust2 file with variable data") {
    ClusterFileHeader header;
    header.metadata["hello world"] = "testing11";

    header.header_fields = ClusterHeader::get_fields();
    header.data_fields = ClusterDataVlen::get_fields();

    ClusterFile<ClusterHeader, ClusterDataVlen> file("/tmp/test_vlen_332211.clust2",
                                                                 "w", header);

    ClusterHeader cluster_header;
    cluster_header.frame_number = 812304;
    cluster_header.n_clusters = 20;
    std::vector<ClusterDataVlen> clusters;
    for (auto i = 0; i < 20; i++) {
        ClusterDataVlen cluster;
        cluster.x = std::vector<int16_t>(i + 1, i);
        cluster.y = std::vector<int16_t>(i + 1, 10 + i);
        cluster.energy = std::vector<int32_t>(i + 1, 20.0 + i);
        clusters.emplace_back(cluster);
    }
    file.write(cluster_header, clusters);
    file.close();

    ClusterFile<ClusterHeader, ClusterDataVlen> file2("/tmp/test_vlen_332211.clust2",
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

TEST_CASE("Read old cluster format") {
    auto fpath = test_data_path() / "clusters" / "single_frame_97_clustrers.clust";
    ClusterFileHeader file_header;
    file_header.header_fields = ClusterHeader::get_fields();
    file_header.data_fields = ClusterData<int32_t,9>::get_fields();
    auto f2 = ClusterFile<ClusterHeader, ClusterData<int32_t,9>>(fpath, "r", file_header, true);
    auto [header, data] = f2.read();
    REQUIRE(header.frame_number == 135);
    REQUIRE(header.n_clusters == 97);
    REQUIRE(data.size() == 97);
    for (int i = 0; i < 97; i++) {
        REQUIRE(data[i].x == 1 + i);
        REQUIRE(data[i].y == 200 + i);
        std::array<int32_t, 9> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        for (int j = 0; j < 9; j++) {
            expected[j] += 9 * i;
        }

        REQUIRE(data[i].array == expected);
    }
}

TEST_CASE("read/write old cluster format"){
    auto fpath = "/tmp/test_old_format.clust";
    ClusterFileHeader file_header;
    file_header.header_fields = ClusterHeader::get_fields();
    file_header.data_fields = ClusterData<int32_t,9>::get_fields();
    auto f2 = ClusterFile<ClusterHeader, ClusterData<int32_t,9>>(fpath, "w", file_header, true);
    for (int i = 0; i < 100; i++) {
        ClusterHeader header;
        header.frame_number = i;
        header.n_clusters = 100 + i;
        std::vector<ClusterData<int32_t,9>> data;
        for (int j = 0; j < header.n_clusters; j++) {
            ClusterData<int32_t,9> d;
            d.x = j;
            d.y = i;
            d.array = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            data.push_back(d);
        }
        f2.write(header, data);
    }
    f2.close();

    auto f3 = ClusterFile<ClusterHeader, ClusterData<int32_t,9>>(fpath, "r", file_header, true);
    for (int i = 0; i < 100; i++) {
        auto [header, data] = f3.read();
        REQUIRE(header.frame_number == i);
        REQUIRE(header.n_clusters == 100 + i);
        for (int j = 0; j < header.n_clusters; j++) {
            REQUIRE(data[j].x == j);
            REQUIRE(data[j].y == i);
            REQUIRE(data[j].array == std::array<int32_t, 9>{1, 2, 3, 4, 5, 6, 7, 8, 9});
        }
    }

}