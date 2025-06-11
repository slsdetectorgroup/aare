#include "aare/utils/ifstream_helpers.hpp"

namespace aare {

std::string ifstream_error_msg(std::ifstream &ifs) {
    std::ios_base::iostate state = ifs.rdstate();
    if (state & std::ios_base::eofbit) {
        return " End of file reached";
    } else if (state & std::ios_base::badbit) {
        return " Bad file stream";
    } else if (state & std::ios_base::failbit) {
        return " File read failed";
    } else {
        return " Unknown/no error";
    }
}

} // namespace aare
