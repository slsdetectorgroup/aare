#include "aare.hpp"

using namespace std;
using namespace aare;

int main() {
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