#include "aare/utils/logger.hpp"

namespace aare {
namespace logger {
namespace internal {
aare::logger::Logger logger_instance = aare::logger::Logger();
} // namespace internal
void set_streams(std::streambuf *out, std::streambuf *err) { internal::logger_instance.set_streams(out, err); }
void set_streams(std::streambuf *out) { internal::logger_instance.set_streams(out); }
void set_verbosity(LOGGING_LEVEL level) { internal::logger_instance.set_verbosity(level); }
Logger &get_logger_instance() { return internal::logger_instance; }
void set_output_file(std::string filename){ internal::logger_instance.set_output_file(filename); }
} // namespace logger
} // namespace aare