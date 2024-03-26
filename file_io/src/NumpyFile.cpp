
// #include "aare/NumpyFile.hpp"

// NumpyFile::NumpyFile(std::filesystem::path fname_){
//     this->fname = fname_;
//     fp = fopen(this->fname.c_str(), "rb");
// }

// Frame NumpyFile::get_frame(size_t frame_number) {
//     if (fp == nullptr) {
//         throw std::runtime_error("File not open");
//     }
//     if (frame_number > header.shape[0]) {
//         throw std::runtime_error("Frame number out of range");
//     }
//     Frame frame = Frame(header.shape[1], header.shape[2], header.dtype.itemsize*8);
//     fseek(fp, header_size + frame_number * bytes_per_frame(), SEEK_SET);
//     fread(frame._get_data(), bytes_per_frame(), 1, fp);
//     return frame;
// }

