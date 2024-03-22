// #include "aare/File.hpp"
// #include "aare/FileFactory.hpp"
// #include <filesystem>
// #include <memory>

// class FileHandler {
//   private:
//     File *f;

//   public:
//     FileHandler(std::filesystem::path fname) {
//         this->f = FileFactory::load_file(fname);
//     }

//     Frame get_frame(int index) { return f->get_frame(index); }

//     // prevent default copying, it can delete the file twice 
//     FileHandler(const FileHandler&) = delete;
//     FileHandler& operator=(const FileHandler&) = delete;

//     ~FileHandler() { delete f; }
// };