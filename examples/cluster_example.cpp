#include "aare/core/defs.hpp"
#include "aare/file_io/ClusterFile.hpp"
#include <iostream>
#include <cassert>  

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "clusters" / "single_frame_97_clustrers.clust");

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
        std::cout << "cluster " << c.x << '\n';
        assert(c.y == offset + 200);
        for (int i = 0; i < 9; i++) {
            assert(c.data[i] == data_offset + i);
        }

        offset++;
        data_offset += 9;


        }

    return 0;
}