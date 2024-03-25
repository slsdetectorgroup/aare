
#include "aare/utils/logger.hpp"
#include <iostream>

namespace aare {
namespace logger {
    LOGGING_LEVEL CURRENT_LEVEL = LOGGING_LEVEL::DEBUG;

namespace internal {
std::ostream standard_output(std::cout.rdbuf());
std::ostream error_output (std::cerr.rdbuf());

} // namespace internal
} // namespace logger
} // namespace aare