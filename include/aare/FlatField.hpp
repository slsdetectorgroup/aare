
/**
 * stores flatfield for angle calibration
 */

#pragma once
#include <cmath>
#include <cstdint>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "MythenDetectorSpecifications.hpp"
#include "NDArray.hpp"

namespace aare {
// TODO maybe template now its uint32
class FlatField {

  public:
    FlatField(std::shared_ptr<MythenDetectorSpecifications> mythen_detector_)
        : mythen_detector(mythen_detector_) {

        flat_field = NDArray<uint32_t, 1>(
            std::array<ssize_t, 1>{mythen_detector->num_strips()}, 0);
    }

    void read_flatfield_from_file(const std::string &filename) {

        std::string word;
        uint32_t strip_number{};

        try {
            std::ifstream file(filename, std::ios_base::in);
            if (!file.good()) {
                throw std::logic_error("file does not exist");
            }

            std::stringstream file_buffer;
            file_buffer << file.rdbuf();

            while (file_buffer >> word) {

                strip_number = std::stoi(word);

                file_buffer >> word;
                if (!mythen_detector->get_bad_channels()[strip_number])
                    flat_field[strip_number] = std::stod(word);
            }

            file.close();
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    NDView<uint32_t, 1> get_flatfield() const { return flat_field.view(); }

    double calculate_mean(double tolerance = 0.001) const {
        auto [sum, count] = std::accumulate(
            flat_field.begin(), flat_field.end(),
            std::make_pair<double, ssize_t>(0.0, 0),
            [&tolerance](std::pair<double, ssize_t> acc, const auto &element) {
                return element == 0 ? acc
                                    : std::make_pair(acc.first + element,
                                                     acc.second + 1);
            });

        return sum / count;
    }

    NDArray<double, 1>
    inverse_normalized_flatfield(double tolerance = 0.001) const {
        double mean = calculate_mean(tolerance);

        NDArray<double, 1> inverse_normalized_flatfield(flat_field.shape());

        for (ssize_t i = 0; i < flat_field.size(); ++i) {
            inverse_normalized_flatfield[i] =
                (flat_field[i] <= tolerance ? 0.0 : mean / flat_field[i]);
            if (inverse_normalized_flatfield[i] < tolerance)
                mythen_detector->get_bad_channels()[i] = true;
        }

        return inverse_normalized_flatfield; // TODO: better to have a copy in
                                             // this context but unneccessary
                                             // for angle calibration code
        // maybe provide inplace and copy option
        // maybe store as member variable access with view
    }

    NDArray<double, 1> normalized_flatfield(double tolerance = 0.001) const {
        double mean = calculate_mean(tolerance);

        NDArray<double, 1> normalized_flatfield(flat_field.shape());

        for (ssize_t i = 0; i < flat_field.size(); ++i) {
            normalized_flatfield[i] = (flat_field[i] == flat_field[i] / mean);
            if (normalized_flatfield[i] < tolerance)
                mythen_detector->get_bad_channels()[i] = true;
        }
        return normalized_flatfield;
    }

  private:
    NDArray<uint32_t, 1> flat_field; // TODO: should be 2d
    std::shared_ptr<MythenDetectorSpecifications> mythen_detector;
};
} // namespace aare