#include "iostream"

namespace aare {

namespace logger {
    enum LOGGING_LEVEL {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3

};
LOGGING_LEVEL CURRENT_LEVEL = LOGGING_LEVEL::DEBUG;
namespace internal {
std::ostream standard_output(std::cout.rdbuf());
std::ostream error_output(std::cerr.rdbuf());

template <LOGGING_LEVEL level> void log_() {
    if (level < CURRENT_LEVEL) {
        return;
    }
    if (level == LOGGING_LEVEL::ERROR) {
        internal::error_output << ' ';
    } else {
        internal::standard_output << ' ';
    }
}
template <LOGGING_LEVEL level, typename First, typename... Strings> void log_(First arg, const Strings... s) {
    if (level < CURRENT_LEVEL) {
        return;
    }
    if (level == LOGGING_LEVEL::ERROR) {
        internal::error_output << static_cast<std::string>(arg) << ' ';
    } else {
        internal::standard_output << static_cast<std::string>(arg) << ' ';
    }
    log_<level>(s...);
}
} // namespace internal





template <LOGGING_LEVEL level, typename... Strings> void log(const Strings... s) {
    if (level < CURRENT_LEVEL) {
        return;
    }
    internal::log_<level>(s...);
}

template <typename... Strings> void debug(const Strings... s) { log<LOGGING_LEVEL::DEBUG>("[DEBUG]", s...); }
template <typename... Strings> void info(const Strings... s) { log<LOGGING_LEVEL::INFO>("[INFO]", s...); }
template <typename... Strings> void warn(const Strings... s) { log<LOGGING_LEVEL::WARNING>("[WARN]", s...); }
template <typename... Strings> void error(const Strings... s) { log<LOGGING_LEVEL::ERROR>("[ERROR]", s...); }
} // namespace logger

} // namespace aare