#include "File.hpp"

File::~File() {
    for (auto& subfile : subfiles) {
        delete subfile;
    }
}
