#pragma once
#include <cmath>
#include <cstdint>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "NDArray.hpp"

namespace aare {

class MythenDetectorSpecifications {

  public:
    // TODO: constructor that reads from a config file

    MythenDetectorSpecifications() {
        num_strips_ = max_modules_ * strips_per_module_;

        num_connected_modules_ = max_modules_;

        bad_channels =
            NDArray<bool, 1>(std::array<ssize_t, 1>{num_strips_}, false);

        connected_modules = NDArray<bool, 1>(
            std::array<ssize_t, 1>{static_cast<ssize_t>(max_modules_)}, true);
    }

    MythenDetectorSpecifications(const size_t max_modules,
                                 const double exposure_time,
                                 const double num_counters = 1,
                                 double bloffset = 1.532)
        : max_modules_(max_modules), num_counters_(num_counters),
          exposure_time_(exposure_time), bloffset_(bloffset) {
        num_strips_ = max_modules_ * strips_per_module_;

        num_connected_modules_ = max_modules_;

        bad_channels =
            NDArray<bool, 1>(std::array<ssize_t, 1>{num_strips_}, false);

        connected_modules = NDArray<bool, 1>(
            std::array<ssize_t, 1>{static_cast<ssize_t>(max_modules_)}, true);
    }

    // TODO templated on filereader
    void read_bad_channels_from_file(const std::string &filename) {
        std::string line;

        try {
            std::ifstream file(filename, std::ios_base::in);
            if (!file.good()) {
                throw std::logic_error("file does not exist");
            }

            while (std::getline(file, line)) {
                std::size_t pos = line.find("-");

                if (pos == std::string::npos) {
                    bad_channels(std::stoi(line)) = true;
                } else {
                    size_t line_size = line.size();
                    for (int i = std::stoi(line.substr(0, pos));
                         i <= std::stoi(line.substr(pos + 1, line_size - pos));
                         ++i)
                        bad_channels(i) = true;
                }
            }

            file.close();
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // TODO template on filereader
    void read_unconnected_modules_from_file(const std::string &filename) {
        std::string line;

        try {
            std::ifstream file(filename, std::ios_base::in);
            if (!file.good()) {
                throw std::logic_error("file does not exist");
            }

            std::stringstream file_buffer;
            file_buffer << file.rdbuf();

            file_buffer >> line;
            num_connected_modules_ -= std::stoi(line);

            while (file_buffer >> line) {
                size_t module = std::stoi(line);
                connected_modules[module] = false;
                for (size_t i = module * strips_per_module_;
                     i < (module + 1) * strips_per_module_; ++i)
                    bad_channels[i] = true;
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    NDView<bool, 1> get_bad_channels() const { return bad_channels.view(); }

    NDView<bool, 1> get_connected_modules() const {
        return connected_modules.view();
    }

    static constexpr double pitch() { return pitch_; }

    static constexpr size_t strips_per_module() { return strips_per_module_; }

    size_t max_modules() const { return max_modules_; }

    size_t num_counters() const { return num_counters_; }

    double exposure_time() const { return exposure_time_; }

    double bloffset() const { return bloffset_; }

    double dtt0() const { return dtt0_; }

    static constexpr double min_angle() { return min_angle_; }

    static constexpr double max_angle() { return max_angle_; }

    ssize_t num_strips() const { return num_strips_; }

  private:
    static constexpr size_t strips_per_module_ = 1280;
    static constexpr double pitch_ = 0.05; // strip width [mm]
    static constexpr double min_angle_ =
        -180.0; // maybe shoudnt be static but configurable
    static constexpr double max_angle_ = 180.0;
    static constexpr double dtt0_ =
        0.0; // No idea what this is - probably configurable

    size_t max_modules_ = 48;

    size_t num_counters_ = 1;

    double exposure_time_ = 5.0; // TODO: could read from acquired file but
                                 // maybe should be configurable
    double bloffset_ = 1.532; // what is this? detector offset relative to what?

    size_t num_connected_modules_{};

    ssize_t num_strips_{};

    NDArray<bool, 1> bad_channels;
    NDArray<bool, 1> connected_modules; // connected modules
};

} // namespace aare