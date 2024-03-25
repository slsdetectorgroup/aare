#include "iostream"
#include <vector>
#define LOCATION std::string(__FILE__)+std::string(":")+std::to_string(__LINE__)+":"+std::string(__func__)+":"

template<typename T>
std::ostream& operator<< (std::ostream& out, const std::vector<T>& v) {
    out << "[";
    size_t last = v.size() - 1;
    for(size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i != last) 
            out << ", ";
    }
    out << "]";
    return out;
}



namespace aare {

namespace logger {
    enum LOGGING_LEVEL {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3

};
 extern LOGGING_LEVEL CURRENT_LEVEL;
namespace internal {
 extern std::ostream standard_output;
 extern std::ostream error_output;

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
        internal::error_output << (arg) << ' ';
    } else {
        internal::standard_output << (arg) << ' ';
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