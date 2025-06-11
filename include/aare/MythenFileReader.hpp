/************************************************
 * @file MythenFileReader.hpp
 * @short minimal file reader to read mythen files
 ***********************************************/

#include <bitset>
#include <filesystem>
#include <string>

#include "Hdf5FileReader.hpp"
#include "NDArray.hpp"

namespace aare {

struct MythenFrame {
    NDArray<uint32_t, 1> photon_counts;
    double detector_angle{};
    // double reference_intensity{}; not needed
    std::array<uint8_t, 3> channel_mask{};
};

/** minimal version for a mythen file reader */
class MythenFileReader : public HDF5FileReader {

  public:
    MythenFileReader(const std::filesystem::path &file_path_,
                     const std::string &file_prefix_)
        : m_base_path(file_path_), file_prefix(file_prefix_) {};

    MythenFrame read_frame(ssize_t frame_index) {
        // TODO not a good design fixed number of digits in file name for frame
        // number -> pad with zeros
        //  not even sure if files have the same name
        std::string current_file_name =
            m_base_path / (file_prefix + std::to_string(frame_index) + ".h5");

        MythenFrame frame;
        open_file(current_file_name);

        auto dataset_photon_count =
            get_dataset("/entry/instrument/detector/data");

        frame.photon_counts =
            dataset_photon_count.store_as_ndarray<uint32_t, 1>();

        ++frame.photon_counts;

        auto dataset_detector_angle =
            get_dataset("/entry/instrument/NDAttributes/DetectorAngle");

        dataset_detector_angle.read_into_buffer(
            reinterpret_cast<std::byte *>(&frame.detector_angle));

        auto dataset_channel_number =
            get_dataset("/entry/instrument/NDAttributes/CounterMask");

        uint8_t channel_number;

        dataset_channel_number.read_into_buffer(
            reinterpret_cast<std::byte *>(&channel_number));

        std::bitset<3> binary_channel_numbers(channel_number); // 1 0 0

        // binary_channel_numbers.flip(); // TODO not sure where most
        // significant
        //  bit is ask Anna again

        frame.channel_mask = std::array<uint8_t, 3>{binary_channel_numbers[0],
                                                    binary_channel_numbers[1],
                                                    binary_channel_numbers[2]};

        close_file();

        return frame;
    }

  private:
    std::filesystem::path m_base_path{};
    std::string file_prefix{};
};

} // namespace aare