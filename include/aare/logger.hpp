// SPDX-License-Identifier: MPL-2.0
#pragma once
/*Utility to log to console*/

#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>

namespace aare {

#define RED      "\x1b[31m"
#define GREEN    "\x1b[32m"
#define YELLOW   "\x1b[33m"
#define BLUE     "\x1b[34m"
#define MAGENTA  "\x1b[35m"
#define CYAN     "\x1b[36m"
#define GRAY     "\x1b[37m"
#define DARKGRAY "\x1b[30m"

#define BG_BLACK   "\x1b[48;5;232m"
#define BG_RED     "\x1b[41m"
#define BG_GREEN   "\x1b[42m"
#define BG_YELLOW  "\x1b[43m"
#define BG_BLUE    "\x1b[44m"
#define BG_MAGENTA "\x1b[45m"
#define BG_CYAN    "\x1b[46m"
#define RESET      "\x1b[0m"
#define BOLD       "\x1b[1m"

enum TLogLevel {
    logERROR,
    logWARNING,
    logINFOBLUE,
    logINFOGREEN,
    logINFORED,
    logINFOCYAN,
    logINFOMAGENTA,
    logINFO,
    logDEBUG, // constructors, destructors etc. should still give too much
              // output
    logDEBUG1,
    logDEBUG2,
    logDEBUG3,
    logDEBUG4,
    logDEBUG5
};

// Compiler should optimize away anything below this value
#ifndef AARE_LOG_LEVEL
#define AARE_LOG_LEVEL                                                         \
    "LOG LEVEL NOT SET IN CMAKE" // This is configured in the main
                                 // CMakeLists.txt
#endif

#define __AT__                                                                 \
    std::string(__FILE__) + std::string("::") + std::string(__func__) +        \
        std::string("(): ")
#define __SHORT_FORM_OF_FILE__ aare::short_file_name(__FILE__)
#define __SHORT_AT__                                                           \
    std::string(__SHORT_FORM_OF_FILE__) + std::string("::") +                  \
        std::string(__func__) + std::string("(): ")

/**
 * @brief Strip the directory part of a path, accepting both '/' and '\\' as
 * separators so that __FILE__ is shortened the same way on every platform.
 */
inline const char *short_file_name(const char *path) noexcept {
    const char *base = path;
    for (const char *p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}

/**
 * @brief Local wall-clock time as HH:MM:SS.mmm (strftime "%X" plus
 * milliseconds), built from std::chrono so it works on every platform.
 */
inline std::string log_timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const time_t t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    tm local{};
#if defined(_MSC_VER)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif

    constexpr size_t buffer_len = 12;
    char buffer[buffer_len];
    if (strftime(buffer, buffer_len, "%X", &local) == 0)
        buffer[0] = '\0';

    constexpr size_t result_len = 100;
    char result[result_len];
    snprintf(result, result_len, "%s.%03d", buffer,
             static_cast<int>(ms.count()));
    return result;
}

class Logger {
    std::ostringstream os;
    TLogLevel m_level = AARE_LOG_LEVEL;

  public:
    Logger() = default;
    explicit Logger(TLogLevel level) : m_level(level) {};
    ~Logger() {
        // output in the destructor to allow for << syntax
        os << RESET << '\n';
        std::clog << os.str() << std::flush; // Single write
    }

    static TLogLevel &
    ReportingLevel() { // singelton eeh TODO! Do we need a runtime option?
        static TLogLevel reportingLevel = logDEBUG5;
        return reportingLevel;
    }

    // Danger this buffer need as many elements as TLogLevel
    static const char *Color(TLogLevel level) noexcept {
        static const char *const colors[] = {
            RED BOLD, YELLOW BOLD, BLUE,  GREEN, RED,   CYAN,  MAGENTA,
            RESET,    RESET,       RESET, RESET, RESET, RESET, RESET};
        // out of bounds
        if (level < 0 || level >= sizeof(colors) / sizeof(colors[0])) {
            return RESET;
        }
        return colors[level];
    }

    // Danger this buffer need as many elements as TLogLevel
    static std::string ToString(TLogLevel level) {
        static const char *const buffer[] = {
            "ERROR",  "WARNING", "INFO",   "INFO",  "INFO",
            "INFO",   "INFO",    "INFO",   "DEBUG", "DEBUG1",
            "DEBUG2", "DEBUG3",  "DEBUG4", "DEBUG5"};
        // out of bounds
        if (level < 0 || level >= sizeof(buffer) / sizeof(buffer[0])) {
            return "UNKNOWN";
        }
        return buffer[level];
    }

    std::ostringstream &Get() {
        os << Color(m_level) << "- " << Timestamp() << " "
           << Logger::ToString(m_level) << ": ";
        return os;
    }

    static std::string Timestamp() { return log_timestamp(); }
};

// TODO! Do we need to keep the runtime option?
#define LOG(level)                                                             \
    if (level > AARE_LOG_LEVEL)                                                \
        ;                                                                      \
    else if (level > aare::Logger::ReportingLevel())                           \
        ;                                                                      \
    else                                                                       \
        aare::Logger(level).Get()

} // namespace aare
