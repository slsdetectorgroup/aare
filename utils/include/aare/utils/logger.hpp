#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#define LOCATION std::string(__FILE__) + std::string(":") + std::to_string(__LINE__) + ":" + std::string(__func__) + ":"

// operator overload to print vectors
// typename T must be printable (i.e. have the << operator)
template <typename T> std::ostream &operator<<(std::ostream &out, const std::vector<T> &v) {
    out << "[";
    size_t last = v.size() - 1;
    for (size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i != last)
            out << ", ";
    }
    out << "]";
    return out;
}

// operator overload for std::array
template <typename T, size_t N> std::ostream &operator<<(std::ostream &out, const std::array<T, N> &v) {
    out << "[";
    size_t last = N - 1;
    for (size_t i = 0; i < N; ++i) {
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

class Logger {

    std::streambuf *standard_buf = std::cout.rdbuf();
    std::streambuf *error_buf = std::cerr.rdbuf();
    std::ostream *standard_output;
    std::ostream *error_output;
    LOGGING_LEVEL VERBOSITY_LEVEL = LOGGING_LEVEL::INFO;

    std::ofstream out_file;

  public:
    void set_output_file(std::string filename) {
        if (out_file.is_open())
            out_file.close();
        out_file.open(filename);
        set_streams(out_file.rdbuf());
    }
    void set_streams(std::streambuf *out, std::streambuf *err) {
        delete standard_output;
        delete error_output;
        standard_output = new std::ostream(out);
        error_output = new std::ostream(err);
    }
    void set_streams(std::streambuf *out) { set_streams(out, out); }
    void set_verbosity(LOGGING_LEVEL level) { VERBOSITY_LEVEL = level; }
    Logger() {
        standard_output = new std::ostream(standard_buf);
        error_output = new std::ostream(error_buf);
    }

    ~Logger() {
        if (out_file.is_open())
            out_file.close();

        standard_output->flush();
        error_output->flush();
        delete standard_output;
        delete error_output;
    }
    template <LOGGING_LEVEL level, typename... Strings> void log(const Strings... s) {
        if (level >= VERBOSITY_LEVEL)
            log_<level>(s...);
    }
    template <typename... Strings> void debug(const Strings... s) { log<LOGGING_LEVEL::DEBUG>("[DEBUG]", s...); }
    template <typename... Strings> void info(const Strings... s) { log<LOGGING_LEVEL::INFO>("[INFO]", s...); }
    template <typename... Strings> void warn(const Strings... s) { log<LOGGING_LEVEL::WARNING>("[WARN]", s...); }
    template <typename... Strings> void error(const Strings... s) { log<LOGGING_LEVEL::ERROR>("[ERROR]", s...); }

  private:
    template <LOGGING_LEVEL level> void log_() {
        if (level == LOGGING_LEVEL::ERROR) {
            *error_output << std::endl;
        } else {
            *standard_output << std::endl;
        }
    }
    template <LOGGING_LEVEL level, typename First, typename... Strings> void log_(First arg, const Strings... s) {
        if (level == LOGGING_LEVEL::ERROR) {
            *error_output << (arg) << ' ';
            error_output->flush();
        } else {
            *standard_output << (arg) << ' ';
            standard_output->flush();
        }
        log_<level>(s...);
    }
};

namespace internal {

extern aare::logger::Logger logger_instance;
} // namespace internal

template <LOGGING_LEVEL level, typename... Strings> void log(const Strings... s) {
    internal::logger_instance.log<level>(s...);
}
template <typename... Strings> void debug(const Strings... s) { internal::logger_instance.debug(s...); }
template <typename... Strings> void info(const Strings... s) { internal::logger_instance.info(s...); }
template <typename... Strings> void warn(const Strings... s) { internal::logger_instance.warn(s...); }
template <typename... Strings> void error(const Strings... s) { internal::logger_instance.error(s...); }

extern void set_streams(std::streambuf *out, std::streambuf *err);
extern void set_streams(std::streambuf *out);
extern void set_verbosity(LOGGING_LEVEL level);
extern void set_output_file(std::string filename);
extern Logger &get_logger_instance();

} // namespace logger

} // namespace aare