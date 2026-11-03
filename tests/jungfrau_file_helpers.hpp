#include "aare/JungfrauDataFile.hpp"
#include "aare/NDArray.hpp"
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <limits>

class TemporaryJungfrauFiles {
  public:
    // TODO: could have an additional argument num files

    /** @brief Create a temporary Jungfrau data file with the given number of
     * frames and a frame generator function.
     * @param num_frames The number of frames to generate.
     * @param frame_generator A function that takes a frame index and returns an
     * NDArray<uint16, 2> representing the frame data.
     */
    explicit TemporaryJungfrauFiles(
        const size_t num_frames,
        std::function<aare::NDArray<uint16_t, 2>(size_t)> frame_generator,
        const size_t num_files = 1) {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() /
                      ("aare-raw-" + std::to_string(unique));

        std::filesystem::create_directory(m_directory);

        // constexpr auto max_ofstream_size =
        // std::numeric_limits<std::ofstream::off_type>::max(); // 562 MB

        size_t frames_per_file = num_frames / num_files;

        for (size_t file_index = 0; file_index < num_files; ++file_index) {

            auto my_file_path = file_path(file_index);
            std::ofstream output(my_file_path, std::ios::binary);
            output.exceptions(std::ios::failbit | std::ios::badbit);

            if (!output.is_open()) {
                throw std::runtime_error("Failed to open output file\n");
            }

            size_t start_frame = file_index * frames_per_file;
            size_t end_frame = (file_index == num_files - 1)
                                   ? num_frames
                                   : start_frame + frames_per_file;

            for (size_t frame = start_frame; frame < end_frame; ++frame) {
                aare::JungfrauDataHeader header{};
                header.framenum = frame;
                const auto frame_data = frame_generator(frame);
                try {
                    output.write(reinterpret_cast<const char *>(&header),
                                 sizeof(header));
                    output.write(
                        reinterpret_cast<const char *>(frame_data.data()),
                        frame_data.total_bytes());
                } catch (const std::ios_base::failure &e) {
                    throw std::runtime_error(fmt::format(
                        "Write failed at frame {}: {}", frame, e.what()));
                }
            }
        }
    }

    TemporaryJungfrauFiles(const TemporaryJungfrauFiles &) = delete;
    TemporaryJungfrauFiles &operator=(const TemporaryJungfrauFiles &) = delete;
    ~TemporaryJungfrauFiles() { std::filesystem::remove_all(m_directory); }

    std::filesystem::path file_path(size_t file_index = 0) const {
        return m_directory /
               fmt::format("{}{:06}.dat", m_base_name, file_index);
    }

  private:
    std::filesystem::path m_directory;
    const std::string m_base_name = "JungfrauTest_";
};