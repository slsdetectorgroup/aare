#pragma once
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>

#include <fstream>
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

// TODO: some of these should be configurable - read them from a config file
class MythenDetectorSpecifications {

  public:
    // TODO: constructor that reads from a config file

    MythenDetectorSpecifications() {
        num_strips_ = max_modules_ * strips_per_module_;

        num_connected_modules_ = max_modules_;

        bad_channels =
            NDArray<bool, 1>(std::array<ssize_t, 1>{num_strips_}, false);

        connected_modules =
            NDArray<bool, 1>(std::array<ssize_t, 1>{max_modules_}, true);
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
                         i < std::stoi(line.substr(pos + 1, line_size - pos));
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

    static constexpr size_t max_modules() { return max_modules_; }

    static constexpr double exposure_time() { return exposure_time_; }

    static constexpr double bloffset() { return bloffset_; }

    static constexpr double dtt0() { return dtt0_; }

    static constexpr double min_angle() { return min_angle_; }

    static constexpr double max_angle() { return max_angle_; }

    ssize_t num_strips() { return num_strips_; }

  private:
    static constexpr size_t max_modules_ = 48;
    static constexpr size_t strips_per_module_ = 1280;
    static constexpr double pitch_ = 0.05; // strip width [mm] ?? TODO: not sure
    static constexpr double min_angle_ = -180.0; // what is this?
    static constexpr double max_angle_ = 180.0;
    static constexpr float ttstep_ =
        0.0036; // probably here to calculate bin size, what is this?

    static constexpr double bloffset_ =
        1.532; // what is this? detector offset relative to what?

    static constexpr double exposure_time_ =
        5.0; // TODO: could read from acquired file but maybe should be
             // configurable

    static constexpr double dtt0_ = 0.0; // No idea what this is

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
            std::array<ssize_t, 1>{mythen_detector->num_strips()});
    }

    void read_flatfield_from_file(const std::string &filename) {

        std::string word;
        uint32_t module_number{};

        try {
            std::ifstream file(filename, std::ios_base::in);
            if (!file.good()) {
                throw std::logic_error("file does not exist");
            }

            std::stringstream file_buffer;
            file_buffer << file.rdbuf();

            while (file_buffer >> word) {

                module_number = std::stoi(word);

                file_buffer >> word;
                if (!mythen_detector->get_bad_channels()[module_number])
                    flat_field[module_number] = std::stod(word);
            }

            file.close();
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    double calculate_mean(double tolerance = 0.001) {
        auto [sum, count] = std::accumulate(
            flat_field.begin(), flat_field.end(),
            std::make_pair<double, ssize_t>(0.0, 0),
            [&tolerance](std::pair<double, ssize_t> acc, const auto &element) {
                return element < tolerance ? acc
                                           : std::make_pair(acc.first + element,
                                                            acc.second + 1);
            });

        return sum / count;
    }

    NDArray<double, 1> inverse_normalized_flatfield(double tolerance = 0.001) {
        double mean = calculate_mean(tolerance);

        NDArray<double, 1> inverse_normalized_flatfield(flat_field.shape());

        std::transform(flat_field.begin(), flat_field.end(),
                       inverse_normalized_flatfield.begin(),
                       [&mean](const auto element) { return mean / element; });

        return inverse_normalized_flatfield; // TODO: better to have a copy in
                                             // this context but unneccessary
                                             // for angle calibration code
        // maybe provide inplace and copy option
        // maybe store as member variable access with view
    }

    NDArray<double, 1> normalized_flatfield(double tolerance = 0.001) {
        double mean = calculate_mean(tolerance);

        NDArray<double, 1> normalized_flatfield(flat_field.shape());

        std::transform(flat_field.begin(), flat_field.end(),
                       normalized_flatfield.begin(),
                       [&mean](const auto element) { return element / mean; });

        return normalized_flatfield;
    }

    // TODO: update is bad channels
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
        centers.reserve(MythenDetectorSpecifications::max_modules());
        conversions.reserve(MythenDetectorSpecifications::max_modules());
        offsets.reserve(MythenDetectorSpecifications::max_modules());
    }

    /** set the histogram bin width [degrees] */
    void set_histogram_bin_width(double bin_width) {
        histogram_bin_width = bin_width;
    }

    double get_histogram_bin_width() { return histogram_bin_width; }

    /** reads the historical Detector Group (DG) parameters from file **/
    void read_initial_calibration_from_file(const std::string &filename);

    /** converts DG parameters to easy EE parameters e.g.geometric parameters */
    parameters convert_to_EE_parameters();

    std::tuple<double, double, double>
    convert_to_EE_parameters(const size_t module_index);

    /** converts DG parameters to BC parameters e.g. best computing
     * parameters */
    parameters convert_to_BC_parameters();

    /** calculates diffraction angle from EE module parameters (used in Beer's
     * Law)
     * @param strip_index local strip index of module
     */
    double diffraction_angle_from_EE_parameters(
        const double normal_distance, const double module_center_distance,
        const double angle, const size_t strip_index);

    /** calculates diffraction angle from EE module parameters (used in Beer's
     * Law)
     * @param strip_index local strip index of module
     */
    double diffraction_angle_from_DG_parameters(const double center,
                                                const double conversion,
                                                const double offset,
                                                const size_t strip_index);

    /** calculated the strip width expressed as angle [degrees]
     * @param strip_index gloabl strip index of detector
     */
    double angular_strip_width(const size_t strip_index);

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
     * redistributes photon counts with of histogram using one bin per strip to
     * histogram with fixed size angle bins
     * @param frame MythenFrame storing data from image
     * @param bin_counts accumulate new photon counts
     * @param new_statistical_weights accumulate new statistical weights
     * @param new_errors accumulate new_errors
     */
    void redistribute_photon_counts_to_fixed_angle_bins(
        const MythenFrame &frame, NDView<double, 1> bin_counts,
        NDView<double, 1> new_statistical_weights,
        NDView<double, 1> new_errors);

  protected:
    // TODO: Design maybe have a struct with three vectors, store all three
    // sets of parameters as member variables

    // TODO: check if interpretation and units are correct
    // historical DG parameters
    // TODO change to NDArray
    std::vector<double> centers; // orthogonal projection of sample onto
                                 // detector (given in strip number) [mm]
                                 // D/pitch
    std::vector<double>
        conversions; // pitch/(normal distance from sample to detector (R)) [mm]
                     // //used for easy conversion
    std::vector<double>
        offsets; // position of strip zero relative to sample [degrees] phi -
                 // 180/pi*D/R TODO: expected an arcsin(D/R)?

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector;

    std::shared_ptr<FlatField> flat_field;

    NDArray<double, 1> new_photon_counts;
    NDArray<double, 1> new_photon_count_errors;

    double histogram_bin_width = 0.0036; // [degrees]

    double exposure_rate =
        1. / MythenDetectorSpecifications::exposure_time(); // TODO change

    std::shared_ptr<MythenFileReader>
        mythen_file_reader; // TODO replace by FileInterface ptr
};

} // namespace aare
