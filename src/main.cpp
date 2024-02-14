// Your First C++ Program
#include <iostream>
#include "file_io/file_factory/FileFactory.hpp"


int main() {
    FileFactory fileFactory=FileFactory::getFactory(std::filesystem::current_path()/"test.json");

    File f = fileFactory.loadFile();
}
