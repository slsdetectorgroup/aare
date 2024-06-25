#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <iostream>

using aare::File;
using aare::FileConfig;
using aare::Frame;

int main() {
    auto path = std::filesystem::path("/tmp/test.npy");
    auto dtype = aare::Dtype(typeid(uint32_t));
    FileConfig const cfg = {dtype, 100, 100};
    File npy(path, "w", cfg);
    Frame f(100, 100, dtype.bitdepth());
    for (int i = 0; i < 10000; i++) {
        f.set<uint32_t>(i / 100, i % 100, i);
    }

    npy.write(f);
    f.set<uint32_t>(0, 0, 77);
    npy.write(f);
    npy.write(f);
    return 0;
}
