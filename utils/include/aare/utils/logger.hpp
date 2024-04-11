#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

/**
 * @brief LOCATION macro to get the current location in the code
 */
#define LOCATION std::string(__FILE__) + std::string(":") + std::to_string(__LINE__) + ":" + std::string(__func__) + ":"

/**
 * @brief operator overload for std::vector
 * @tparam T type of the vector. T should have operator<< defined
 * @param out output stream
 * @param v vector to print
 * @return std::ostream& output stream
 * @note this is used to print vectors in the logger (or anywhere else)
 */
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

/**
 * @brief operator overload for std::array
 * @tparam T type of the array. T should have operator<< defined
 * @tparam N size of the array
 * @param out output stream
 * @param v array to print
 * @return std::ostream& output stream
 *
 */
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

/**
 * @brief operator overload for std::map
 * @tparam K type of the key in the map. K should have operator<< defined
 * @tparam V type of the value in the map. V should have operator<< defined
 * @param out output stream
 * @param v map to print
 * @return std::ostream& output stream
 *
 */
template <typename K, typename V> std::ostream &operator<<(std::ostream &out, const std::map<K, V> &v) {
    out << "{";
    size_t i = 0;
    for (auto &kv : v) {
        out << kv.first << ": " << kv.second << ((++i != v.size()) ? ", " : "");
    }

    out << "}";
    return out;
}

namespace aare {

namespace logger {
/**
 * @brief enum to define the logging level
 */
enum LOGGING_LEVEL {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3

};

/**
 * @brief Logger class to log messages
 * @note can be used to log to file or to a std::streambuf (like std::cout)
 * @note by default logs to std::cout and std::cerr with INFO verbosity
 */
class Logger {

  public:
    /**
     * @brief get the instance of the logger
     */
    Logger() {
        standard_output = new std::ostream(standard_buf);
        error_output = new std::ostream(error_buf);
    }

    /**
     * @brief set the output file for the logger by filename
     * @param filename name of the file to log to
     * @return void
     */
    void set_output_file(std::string filename) {
        if (out_file.is_open())
            out_file.close();
        out_file.open(filename);
        set_streams(out_file.rdbuf());
    }

    /**
     * @brief set the output streams for the logger
     * @param out output stream for standard output
     * @param err output stream for error output
     * @return void
     */
    void set_streams(std::streambuf *out, std::streambuf *err) {
        delete standard_output;
        delete error_output;
        standard_output = new std::ostream(out);
        error_output = new std::ostream(err);
    }
    /**
     * @brief set the output streams for the logger
     * @param out output stream for both standard and error output
     * @return void
     */
    void set_streams(std::streambuf *out) { set_streams(out, out); }

    /**
     * @brief set the verbosity level of the logger
     * @param level verbosity level
     */
    void set_verbosity(LOGGING_LEVEL level) { VERBOSITY_LEVEL = level; }

    /**
     * @brief destructor for the logger
     * @note closes the file if it is open
     * @note flushes the output streams
     */
    ~Logger() {
        if (out_file.is_open())
            out_file.close();

        standard_output->flush();
        error_output->flush();
        delete standard_output;
        delete error_output;
    }

    /**
     * @brief log a message
     * @tparam level logging level
     * @tparam Strings variadic template for inferring the types of the arguments (not necessarily strings but can be
     * any printable type)
     * @param s arguments to log
     * @return void
     */
    template <LOGGING_LEVEL level, typename... Strings> void log(const Strings... s) {
        if (level >= VERBOSITY_LEVEL)
            log_<level>(s...);
    }
    /**
     * @brief log a message with DEBUG level
     */
    template <typename... Strings> void debug(const Strings... s) { log<LOGGING_LEVEL::DEBUG>("[DEBUG]", s...); }
    /**
     * @brief log a message with INFO level
     */
    template <typename... Strings> void info(const Strings... s) { log<LOGGING_LEVEL::INFO>("[INFO]", s...); }
    /**
     * @brief log a message with WARNING level
     */
    template <typename... Strings> void warn(const Strings... s) { log<LOGGING_LEVEL::WARNING>("[WARN]", s...); }
    /**
     * @brief log a message with ERROR level
     */
    template <typename... Strings> void error(const Strings... s) { log<LOGGING_LEVEL::ERROR>("[ERROR]", s...); }

  private:
    std::streambuf *standard_buf = std::cout.rdbuf();
    std::streambuf *error_buf = std::cerr.rdbuf();
    std::ostream *standard_output;
    std::ostream *error_output;
    LOGGING_LEVEL VERBOSITY_LEVEL = LOGGING_LEVEL::INFO;

    std::ofstream out_file;
    /**
     * @brief log_ function private function to log messages
     * @tparam level logging level
     * @note this is the final function in the recursive template function log_
     * @note this function is called when there are no more arguments to log
     * @note adds a newline at the end of the log message
     */
    template <LOGGING_LEVEL level> void log_() {
        if (level == LOGGING_LEVEL::ERROR) {
            *error_output << std::endl;
        } else {
            *standard_output << std::endl;
        }
    }

    /**
     * @brief log_ recursive function private function to log messages
     * @tparam level logging level
     * @tparam First type of the first argument
     * @tparam Strings variadic template for inferring the types of the arguments
     * @param arg first argument to log
     * @param s rest of the arguments to log
     * @note called at first from the public log function
     */
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
/**
 * @brief global instance of the logger
 */
extern aare::logger::Logger logger_instance;
} // namespace internal

/**
 * functions below are the public interface to the logger.
 * These functions call the corresponding functions in the logger_instance
 * @note this is done to avoid having to pass the logger_instance around and allow users to assign their own
 * logger_instance
 */

/**
 * @brief log a message with the given level
 * @tparam level logging level
 * @tparam Strings variadic template for inferring the types of the arguments
 * @param s arguments to log
 * @return void
 */
template <LOGGING_LEVEL level, typename... Strings> void log(const Strings... s) {
    internal::logger_instance.log<level>(s...);
}
/**
 * @brief log a message with DEBUG level
 */
template <typename... Strings> void debug(const Strings... s) { internal::logger_instance.debug(s...); }
/**
 * @brief log a message with INFO level
 */
template <typename... Strings> void info(const Strings... s) { internal::logger_instance.info(s...); }
/**
 * @brief log a message with WARNING level
 */
template <typename... Strings> void warn(const Strings... s) { internal::logger_instance.warn(s...); }
/**
 * @brief log a message with ERROR level
 */
template <typename... Strings> void error(const Strings... s) { internal::logger_instance.error(s...); }

extern void set_streams(std::streambuf *out, std::streambuf *err);
extern void set_streams(std::streambuf *out);
extern void set_verbosity(LOGGING_LEVEL level);
extern void set_output_file(std::string filename);
extern Logger &get_logger_instance();

} // namespace logger

} // namespace aare