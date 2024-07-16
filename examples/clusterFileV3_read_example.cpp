#include "aare.hpp"
#include "aare/examples/defs.hpp"

using namespace aare;

int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    // std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" /
    //                                   "test_cluster.clust2");
    std::filesystem::path const fpath("/tmp/test_cluster2.clust2");
    v3::ClusterFile<v3::ClusterHeader,v3::ClusterData<>> file(fpath, "r");
    auto result = file.read();

    std::cout << result.header.to_string() << std::endl;
    std::cout << result.data.size() << '\n';
    for (auto &c : result.data) {
        std::cout << c.to_string() << '\n';
    }
    result = file.read();

    std::cout << result.header.to_string() << std::endl;
    std::cout << result.data.size() << '\n';
    for (auto &c : result.data) {
        std::cout << c.to_string() << '\n';
    }
}