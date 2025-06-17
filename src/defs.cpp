#include "aare/defs.hpp"

#include <fmt/core.h>
namespace aare {

void assert_failed(const std::string &msg) {
    fmt::print(msg);
    exit(1);
}

} // namespace aare