#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace aare {

/**
 * @brief Compare two files
 *
 * @param p1 path to the first file
 * @param p2 path to the second file
 * @return true if the files are the same, false otherwise
 */
bool inline compare_files(const std::string &p1, const std::string &p2) {
    std::ifstream f1(p1, std::ifstream::binary | std::ifstream::ate);
    std::ifstream f2(p2, std::ifstream::binary | std::ifstream::ate);

    if (f1.fail() || f2.fail()) {
        return false; // file problem
    }

    if (f1.tellg() != f2.tellg()) {
        return false; // size mismatch
    }

    // seek back to beginning and use std::equal to compare contents
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);
    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2.rdbuf()));
}
bool inline compare_files(const std::filesystem::path &p1, const std::filesystem::path &p2) {
    return compare_files(p1.string(), p2.string());
}
} // namespace aare