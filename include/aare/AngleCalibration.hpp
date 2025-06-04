#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "MythenFileReader.hpp"
#include "NDArray.hpp"

namespace aare {

using parameters =
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;

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
                                 const double bloffset)
        : max_modules_(max_modules), exposure_time_(exposure_time),
          bloffset_(bloffset) {
        num_strips_ = max_modules_ * strips_per_module_;

        num_connected_modules_ = max_modules_;

        bad_channels =
            NDArray<bool, 1>(std::array<ssize_t, 1>{num_strips_}, false);

        connected_modules = NDArray<bool, 1>(
            std::array<ssize_t, 1>{static_cast<ssize_t>(max_modules_)}, true);
    }

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

    NDView<bool, 1> get_bad_channels() { return bad_channels.view(); }

    NDView<bool, 1> get_connected_modules() { return connected_modules.view(); }

    static constexpr double pitch() { return pitch_; }

    static constexpr size_t strips_per_module() { return strips_per_module_; }

    size_t max_modules() { return max_modules_; }

    double exposure_time() { return exposure_time_; }

    double bloffset() { return bloffset_; }

    double dtt0() { return dtt0_; }

    static constexpr double min_angle() { return min_angle_; }

    static constexpr double max_angle() { return max_angle_; }

    ssize_t num_strips() { return num_strips_; }

  private:
    static constexpr size_t strips_per_module_ = 1280;
    static constexpr double pitch_ = 0.05; // strip width [mm]
    static constexpr double min_angle_ =
        -180.0; // maybe shoudnt be static but configurable
    static constexpr double max_angle_ = 180.0;
    static constexpr double dtt0_ =
        0.0; // No idea what this is - probably configurable

    size_t max_modules_ = 48;

    double exposure_time_ = 5.0; // TODO: could read from acquired file but
                                 // maybe should be configurable
    double bloffset_ = 1.532; // what is this? detector offset relative to what?

    size_t num_connected_modules_{};

    ssize_t num_strips_{};

    NDArray<bool, 1> bad_channels;
    NDArray<bool, 1> connected_modules; // connected modules
};

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

    NDView<uint32_t, 1> get_flatfield() { return flat_field.view(); }

    double calculate_mean(double tolerance = 0.001) {
        auto [sum, count] = std::accumulate(
            flat_field.begin(), flat_field.end(),
            std::make_pair<double, ssize_t>(0.0, 0),
            [&tolerance](std::pair<double, ssize_t> acc, const auto &element) {
                return element == 0 ? acc
                                    : std::make_pair(acc.first + element,
                                                     acc.second + 1);
            });

        std::cout << "sum: " << sum << std::endl;
        std::cout << "count: " << count << std::endl;
        return sum / count;
    }

    NDArray<double, 1> inverse_normalized_flatfield(double tolerance = 0.001) {
        double mean = calculate_mean(tolerance);

        std::cout << "mean: " << mean << std::endl;

        NDArray<double, 1> inverse_normalized_flatfield(flat_field.shape());

        /*
        std::transform(flat_field.begin(), flat_field.end(),
                       inverse_normalized_flatfield.begin(),
                       [&mean](const auto element) {
                           return element == 0 ? 0.0 : mean / element;
                       });
        */

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

    NDArray<double, 1> normalized_flatfield(double tolerance = 0.001) {
        double mean = calculate_mean(tolerance);

        NDArray<double, 1> normalized_flatfield(flat_field.shape());

        /*
        std::transform(flat_field.begin(), flat_field.end(),
                       normalized_flatfield.begin(),
                       [&mean](const auto element) { return element / mean; });
        */

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

class AngleCalibration {

  public:
    AngleCalibration(
        std::shared_ptr<MythenDetectorSpecifications> mythen_detector_,
        std::shared_ptr<FlatField> flat_field_,
        std::shared_ptr<MythenFileReader> mythen_file_reader_)
        : mythen_detector(mythen_detector_), flat_field(flat_field_),
          mythen_file_reader(mythen_file_reader_) {
        centers.reserve(mythen_detector->max_modules());
        conversions.reserve(mythen_detector->max_modules());
        offsets.reserve(mythen_detector->max_modules());

        exposure_rate = 1. / mythen_detector->exposure_time();

        num_bins =
            std::floor(mythen_detector->max_angle() / histogram_bin_width) -
            std::floor(mythen_detector->min_angle() / histogram_bin_width) +
            1; // TODO only works if negative
               // and positive angle
    }

    /** set the histogram bin width [degrees] */
    void set_histogram_bin_width(double bin_width) {
        histogram_bin_width = bin_width;

        num_bins =
            std::floor(mythen_detector->max_angle() / histogram_bin_width) -
            std::floor(mythen_detector->min_angle() / histogram_bin_width) +
            1; // TODO only works if negative
               // and positive angle
    }

    double get_histogram_bin_width() { return histogram_bin_width; }

    /** reads the historical Detector Group (DG) parameters from file **/
    void read_initial_calibration_from_file(const std::string &filename);

    std::vector<double> get_centers() { return centers; }

    std::vector<double> get_conversions() { return conversions; }

    std::vector<double> get_offsets() { return offsets; }

    /** converts DG parameters to easy EE parameters e.g.geometric
     * parameters */
    parameters convert_to_EE_parameters();

    std::tuple<double, double, double>
    convert_to_EE_parameters(const size_t module_index);

    std::tuple<double, double, double>
    convert_to_EE_parameters(const double center, const double conversion,
                             const double offset);

    /** converts DG parameters to BC parameters e.g. best computing
     * parameters */
    parameters convert_to_BC_parameters();

    /** calculates diffraction angle from EE module parameters (used in
     * Beer's Law)
     * @param strip_index local strip index of module
     */
    double diffraction_angle_from_EE_parameters(
        const double module_center_distance, const double normal_distance,
        const double angle, const size_t strip_index,
        const double distance_to_strip = 0);

    /** calculates diffraction angle from EE module parameters (used in
     * Beer's Law)
     * @param center module center
     * @param conversion module conversion
     * @param offset module offset
     * @param strip_index local strip index of module
     * @param distance_to_strip distance to strip given by strip_index and
     * module -> note needs to be small enough to be in the respective module
     */
    double diffraction_angle_from_DG_parameters(
        const double center, const double conversion, const double offset,
        const size_t strip_index, const double distance_to_strip = 0);

    /** calculated the strip width expressed as angle [degrees]
     * @param strip_index gloabl strip index of detector
     */
    double angular_strip_width_from_DG_parameters(const size_t strip_index);

    double angular_strip_width_from_DG_parameters(const double center,
                                                  const double conversion,
                                                  const double offset,
                                                  const size_t strip_index);

    double angular_strip_width_from_EE_parameters(const size_t strip_index);

    double angular_strip_width_from_EE_parameters(
        const double module_center_distance, const double normal_distance,
        const double angle, const size_t strip_index);

    /** converts global strip index to local strip index of that module */
    size_t
    global_to_local_strip_index_conversion(const size_t global_strip_index);

    /**
     * calculates new histogram with fixed sized angle bins
     * for several acquisitions at different detector angles for given frame
     * indices
     * @param start_frame_index, end_frame_index gives range of frames
     */
    void
    calculate_fixed_bin_angle_width_histogram(const size_t start_frame_index,
                                              const size_t end_frame_index);

    /**
     * redistributes photon counts with of histogram using one bin per strip
     * to histogram with fixed size angle bins
     * @param frame MythenFrame storing data from image
     * @param bin_counts accumulate new photon counts
     * @param new_statistical_weights accumulate new statistical weights
     * @param new_errors accumulate new_errors
     */
    void redistribute_photon_counts_to_fixed_angle_bins(
        const MythenFrame &frame, NDView<double, 1> bin_counts,
        NDView<double, 1> new_statistical_weights, NDView<double, 1> new_errors,
        NDView<double, 1> inverse_nromalized_flatfield);

    void write_to_file(const std::string &filename);

  protected:
    // TODO: Design maybe have a struct with three vectors, store all three
    // sets of parameters as member variables

    // TODO: check if interpretation and units are correct
    // historical DG parameters
    // TODO change to NDArray
    std::vector<double> centers;     // orthogonal projection of sample onto
                                     // detector (given in strip number) [mm]
                                     // D/pitch
    std::vector<double> conversions; // pitch/(normal distance from sample
                                     // to detector (R)) [mm]
                                     // //used for easy conversion
    std::vector<double>
        offsets; // position of strip zero relative to sample [degrees] phi
                 // - 180/pi*D/R TODO: expected an arcsin(D/R)?

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector;

    std::shared_ptr<FlatField> flat_field;

    NDArray<double, 1> new_photon_counts;
    NDArray<double, 1> new_photon_count_errors;

    double histogram_bin_width = 0.0036; // [degrees]

    ssize_t num_bins{};

    double exposure_rate{};

    std::shared_ptr<MythenFileReader>
        mythen_file_reader; // TODO replace by FileInterface ptr
};

} // namespace aare
