#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" / "single_frame_97_clustrers.clust");

    // reading a file
    aare::ClusterFile cf(fpath, "r");
    std::cout << "file opened " << '\n';
    std::cout << "n_clusters " << cf.count() << '\n';
    std::cout << "frame_number " << cf.frame() << '\n';
    std::cout << "file size: " << cf.count() << '\n';
    cf.seek(0); // seek to the beginning of the file (this is the default behavior of the constructor)

    auto cluster = cf.read(97);

    std::cout << "read 10 clusters" << '\n';
    int offset = 0;
    int data_offset = 0;
    for (auto c : cluster) {
        assert(c.y == offset + 200);
        for (int i = 0; i < 9; i++) {
            assert(c.data[i] == data_offset + i);
        }

        offset++;
        data_offset += 9;
    }

    // writing a file
    std::filesystem::path const fpath_out("/tmp/cluster_example_file.clust");
    aare::ClusterFile cf_out(fpath_out, "w", ClusterFileConfig(1, 44));
    std::cout << "file opened for writing" << '\n';
    std::vector<Cluster> clusters;
    for (int i = 0; i < 1084; i++) {
        Cluster c;
        c.x = i;
        c.y = i + 200;
        for (int j = 0; j < 9; j++) {
            c.data[j] = j;
        }
        clusters.push_back(c);
    }
    cf_out.write(clusters);
    std::cout << "wrote 10 clusters" << '\n';
    cf_out.update_header();

    return 0;
}