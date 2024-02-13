// Your First C++ Program
#include <iostream>
#include "file_io/FileFactory.hpp"


int main() {
    FileFactory fileFactory=FileFactory();
    std::filesystem::path p = std::filesystem::current_path();
    File f = fileFactory.loadFile(p/"test.raw");
}
