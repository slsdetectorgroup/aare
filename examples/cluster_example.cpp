#include "aare.hpp"
#include "aare/examples/defs.hpp"

#include <cassert>
#include <iostream>

using namespace aare;
int main() {
    auto PROJECT_ROOT_DIR = std::filesystem::path(getenv("AARE_ROOT_DIR"));
    std::filesystem::path const fpath(PROJECT_ROOT_DIR / "data" / "jungfrau" / "jungfrau_single_master_0.json");
    auto f = File(fpath, "r");
    auto frame = f.iread(0);
    auto f00 = *(uint16_t *)frame.get(0, 0);
    auto f01 = *(uint16_t *)frame.get(0, 1);
    auto f10 = *(uint16_t *)frame.get(1, 0);
    auto f11 = *(uint16_t *)frame.get(1, 1);
    std::cout << f00 << " " << f01 << std::endl;
    std::cout << f10 << " " << f11 << std::endl;

    std::cout << "-----------------" << std::endl;
    Pedestal p = Pedestal(frame.rows(), frame.cols());
    p.push(0,0,f00);
    p.push(0,1,f01);
    p.push(1,0,f10);
    p.push(1,1,f11);

    std::cout<<p.mean(0,0)<<"\n "<<p.mean(0,1)<<std::endl;
    std::cout<<p.mean(1,0)<<"\n "<<p.mean(1,1)<<std::endl;
}