#include "aare/string_utils.hpp"

#include <algorithm>

namespace aare {

std::string RemoveUnit(std::string &str) {
    auto it = str.begin();
    while (it != str.end()) {
        if (std::isalpha(*it))
            break;
        ++it;
    }
    auto pos = it - str.begin();
    auto unit = str.substr(pos);
    str.erase(it, end(str));
    return unit;
}

void TrimWhiteSpaces(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
                return !std::isspace(c);
            }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            s.end());
}

} // namespace aare