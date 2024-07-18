#include "aare.hpp"

using namespace std;
using namespace aare;

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" /
                                      "single_frame_97_clustrers.clust");
    auto f = deprecated::ClusterFile(fpath, "r");
    auto frame_number = f.frame_number();
    auto clusters = f.read();
    std::cout << "READING WITH OLD CLUSTER FILE READER" << std::endl;
    std::cout << "frame number: " << frame_number << std::endl;
    std::cout << "number of clusters: " << clusters.size() << std::endl;
    for (auto &cluster : clusters) {
        cout << cluster.to_string() << endl;
    }
    std::cout << "\n\n";
    std::cout << "READING WITH NEW CLUSTER FILE READER" << std::endl;
    ClusterFileHeader file_header;
    file_header.header_fields = ClusterHeader::get_fields();
    file_header.data_fields = ClusterData<int32_t, 9>::get_fields();
    auto f2 = ClusterFile<ClusterHeader, ClusterData<>>("/tmp/test_old_format.clust", "r",
                                                        file_header, true);
    auto [header, data] = f2.read();
    std::cout << header.to_string() << std::endl;
    std::cout << data.size() << '\n';

    for (auto &c : data) {
        std::cout << c.to_string() << std::endl;
    }
    auto [header2, data2] = f2.read();
    std::cout << header2.to_string() << std::endl;
    std::cout << data2.size() << '\n';

    for (auto &c : data2) {
        std::cout << c.to_string() << std::endl;
    }
    return 0;
}