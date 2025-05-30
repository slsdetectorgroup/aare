#pragma once
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Hdf5FileReader.hpp"
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

struct MythenFrame {
    NDArray<uint32_t, 1> photon_counts;
    double detector_angle{};
    // double reference_intensity{}; not needed
    std::array<bool, 3> channel_mask{};
};

/** minimal version for a mythen file reader */
class MythenFileReader : public HDF5FileReader {

  public:
    MythenFileReader(const std::filesystem::path &file_path_,
                     const std::string &file_prefix_)
        : m_base_path(file_path_), file_prefix(file_prefix_) {};

    MythenFrame read_frame(ssize_t frame_index) {
        std::string current_file_name = m_base_path.string() + file_prefix +
                                        std::to_string(frame_index) + ".h5";

        open_file(current_file_name);

        auto dataset_photon_count =
            get_dataset("/entry/instrument/detector/data");

        NDArray photon_counts =
            dataset_photon_count.store_as_ndarray<uint32_t, 1>();

        auto dataset_detector_angle =
            get_dataset("/entry/instrument/NDAttributes/DetectorAngle");

        double detector_angle;

        dataset_detector_angle.read_into_buffer(
            reinterpret_cast<std::byte *>(&detector_angle));

        auto dataset_channel_number =
            get_dataset("/entry/instrument/NDAttributes/CounterMask");

        uint8_t channel_number;

        dataset_channel_number.read_into_buffer(
            reinterpret_cast<std::byte *>(&channel_number));

        std::bitset<3> binary_channel_numbers(channel_number); // 1 0 0

        // binary_channel_numbers.flip(); // TODO not sure where most
        // significant
        //  bit is ask Anna again

        std::array<bool, 3> channel_mask{binary_channel_numbers[2],
                                         binary_channel_numbers[1],
                                         binary_channel_numbers[0]};

        close_file();

        return MythenFrame{photon_counts, detector_angle, channel_mask};
    }

  private:
    std::filesystem::path m_base_path{};
    std::string file_prefix{};
};

class AngleCalibration {

  public:
    AngleCalibration(
        std::shared_ptr<MythenDetectorSpecifications> mythen_detector_,
        std::shared_ptr<FlatField> flat_field_)
        : mythen_detector(mythen_detector_), flat_field(flat_field_) {
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
    global_to_local_strip_index_conversion(const size_t global_strip_index) {
        const size_t module_index =
            global_strip_index /
            MythenDetectorSpecifications::strips_per_module();
        // local strip index in module
        size_t local_strip_index =
            global_strip_index -
            module_index * MythenDetectorSpecifications::strips_per_module();
        // switch if indexing is in clock-wise direction
        local_strip_index =
            std::signbit(conversions[module_index])
                ? MythenDetectorSpecifications::strips_per_module() -
                      local_strip_index
                : local_strip_index;

        return local_strip_index;
    }

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

void AngleCalibration::read_initial_calibration_from_file(
    const std::string &filename) {

    std::string line;
    uint32_t module_number{};

    try {
        std::ifstream file(filename, std::ios_base::in);
        if (!file.good()) {
            throw std::logic_error("file does not exist");
        }

        std::stringstream file_buffer;
        file_buffer << file.rdbuf();

        while (file_buffer >> line) {
            if (line == "module") {
                file_buffer >> line;
                module_number = std::stoi(line);
            }
            if (line == "center") {
                file_buffer >> line;
                centers.insert(centers.begin() + module_number,
                               std::stod(line));
            }
            if (line == "conversion") {
                file_buffer >> line;
                conversions.insert(conversions.begin() + module_number,
                                   std::stod(line));
            }
            if (line == "offset") {
                file_buffer >> line;
                offsets.insert(offsets.begin() + module_number,
                               std::stod(line));
            }
        }

        file.close();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

parameters AngleCalibration::convert_to_EE_parameters() {

    // normal distance between sample and detector (R)
    std::vector<double> normal_distances(centers.size());
    // distances between intersection point of sample normal and module origin
    // (D)
    std::vector<double> module_center_distances(centers.size());
    // angles between undiffracted beam and orthogonal sample projection on
    // detector (phi)
    std::vector<double> angles(centers.size());

    for (size_t i = 0; i < centers.size(); ++i) {
        auto [normal_distance, module_center_distance, angle] =
            convert_to_EE_parameters(i);
        normal_distances[i] = normal_distance;
        module_center_distances[i] = module_center_distance;
        angles[i] = angle;
    }

    return std::make_tuple(normal_distances, module_center_distances, angles);
}

std::tuple<double, double, double>
AngleCalibration::convert_to_EE_parameters(const size_t module_index) {
    const double normal_distance =
        centers[module_index] * MythenDetectorSpecifications::pitch();
    const double module_center_distance =
        MythenDetectorSpecifications::pitch() /
        std::abs(conversions[module_index]);
    const double angle =
        offsets[module_index] + 180.0 / M_PI * centers[module_index] *
                                    std::abs(conversions[module_index]);

    return std::make_tuple(normal_distance, module_center_distance, angle);
}

/*
parameters
AngleCalibration::convert_to_BC_parameters() {}
*/

double AngleCalibration::diffraction_angle_from_DG_parameters(
    const double center, const double conversion, const double offset,
    const size_t strip_index) {
    return offset + 180.0 / M_PI *
                        (center * conversion -
                         atan((center - strip_index) * conversion));
}

double AngleCalibration::diffraction_angle_from_EE_parameters(
    const double normal_distance, const double module_center_distance,
    const double angle, const size_t strip_index) {

    return angle -
           180.0 / M_PI *
               atan((module_center_distance -
                     MythenDetectorSpecifications::pitch() * strip_index) /
                    normal_distance); // TODO: why is it minus
                                      // is it defined counter
                                      // clockwise? thought
                                      // should have a flipped
                                      // sign
}

double AngleCalibration::angular_strip_width(const size_t strip_index) {

    const size_t module_index =
        strip_index / MythenDetectorSpecifications::strips_per_module();

    const auto [normal_distance, module_center_distance, angle] =
        convert_to_EE_parameters(module_index);

    const size_t local_strip_index =
        global_to_local_strip_index_conversion(strip_index);

    return 180.0 / M_PI *
           std::abs(diffraction_angle_from_EE_parameters(
                        normal_distance, module_center_distance, angle,
                        local_strip_index - 0.5) -
                    diffraction_angle_from_EE_parameters(
                        normal_distance, module_center_distance, angle,
                        local_strip_index + 0.5));
    // TODO: again not sure about division order - taking abs anyway
}

void AngleCalibration::calculate_fixed_bin_angle_width_histogram(
    const size_t start_frame_index, const size_t end_frame_index) {

    ssize_t num_bins = mythen_detector->max_angle() / histogram_bin_width -
                       mythen_detector->min_angle() /
                           histogram_bin_width; // TODO only works if negative
                                                // and positive angle
    new_photon_counts = NDArray<double, 1>(std::array<ssize_t, 1>{num_bins});

    new_photon_count_errors =
        NDArray<double, 1>(std::array<ssize_t, 1>{num_bins});

    NDArray<double, 1> bin_counts(std::array<ssize_t, 1>{num_bins}, 0.0);
    NDArray<double, 1> new_statistical_weights(std::array<ssize_t, 1>{num_bins},
                                               1.0);

    NDArray<double, 1> new_errors(std::array<ssize_t, 1>{num_bins}, 0.0);

    for (size_t frame_index = start_frame_index; frame_index < end_frame_index;
         ++frame_index) {
        MythenFrame frame = mythen_file_reader->read_frame(frame_index);
        redistribute_photon_counts_to_fixed_angle_bins(
            frame, new_photon_counts.view(), new_statistical_weights.view(),
            new_errors.view());
    }

    for (ssize_t i = 0; i < new_photon_counts.size(); ++i) {
        new_photon_counts[i] = bin_counts[i] / new_statistical_weights[i];
        new_photon_count_errors[i] = 1.0 / std::sqrt(bin_counts[i]);
    }
}

void AngleCalibration::redistribute_photon_counts_to_fixed_angle_bins(
    const MythenFrame &frame, NDView<double, 1> bin_counts,
    NDView<double, 1> new_statistical_weights, NDView<double, 1> new_errors) {

    ssize_t channel = 0; // TODO handle mask - FlatField still 1d

    if (frame.photon_counts.shape()[0] != mythen_detector->num_strips()) {
        throw std::runtime_error("wrong number of strips read");
    }

    NDArray<double, 1> inverse_normalized_flatfield =
        flat_field->inverse_normalized_flatfield();

    ssize_t num_bins1 = mythen_detector->min_angle() / histogram_bin_width;
    ssize_t num_bins2 = mythen_detector->max_angle() / histogram_bin_width;

    for (ssize_t strip_index = 0; strip_index < mythen_detector->num_strips();
         ++strip_index) {

        size_t module_index =
            strip_index / MythenDetectorSpecifications::strips_per_module();

        if (mythen_detector->get_bad_channels()[strip_index] ||
            !mythen_detector->get_connected_modules()[module_index])
            continue;

        double poisson_error = std::sqrt(frame.photon_counts(strip_index)) *
                               inverse_normalized_flatfield(strip_index) *
                               exposure_rate; // not sure what this is
        double corrected_photon_count =
            frame.photon_counts(strip_index) *
            inverse_normalized_flatfield(strip_index) * exposure_rate;

        size_t local_strip_index =
            global_to_local_strip_index_conversion(strip_index);

        double diffraction_angle = diffraction_angle_from_DG_parameters(
            centers[module_index], conversions[module_index],
            offsets[module_index], local_strip_index);

        diffraction_angle += (frame.detector_angle + mythen_detector->dtt0() +
                              mythen_detector->bloffset());

        if (diffraction_angle < mythen_detector->min_angle() ||
            diffraction_angle > mythen_detector->max_angle())
            continue;

        double angle_covered_by_strip = angular_strip_width(strip_index);

        double photon_count_per_bin = histogram_bin_width *
                                      corrected_photon_count /
                                      angle_covered_by_strip;
        double error_photon_count_per_bin =
            histogram_bin_width * poisson_error / angle_covered_by_strip;

        double statistical_weights =
            1.0 / std::pow(error_photon_count_per_bin, 2); // 1./sigma²

        double strip_boundary_left =
            diffraction_angle - 0.5 * angle_covered_by_strip;
        double strip_boundary_right =
            diffraction_angle + 0.5 * angle_covered_by_strip;

        ssize_t left_bin_index = std::max(
            num_bins1,
            static_cast<ssize_t>(
                std::floor(strip_boundary_left / histogram_bin_width) - 1));
        ssize_t right_bin_index = std::min(
            num_bins2,
            static_cast<ssize_t>(
                std::ceil(strip_boundary_right / histogram_bin_width) + 1));

        // TODO should it be < or <=
        for (ssize_t bin = left_bin_index; bin <= right_bin_index; ++bin) {
            double bin_coverage = std::min(strip_boundary_right,
                                           (bin + 0.5) * histogram_bin_width) -
                                  std::max(strip_boundary_left,
                                           (bin - 0.5) * histogram_bin_width);

            double bin_coverage_factor = bin_coverage / histogram_bin_width;

            ssize_t bin_index = bin - num_bins1;
            if (bin_coverage > 0.0001) {
                new_statistical_weights(bin_index) +=
                    statistical_weights * bin_coverage_factor -
                    1.0; //- 1 to avoid division by zero - initiallized with 1.
                bin_counts(bin_index) += statistical_weights *
                                         bin_coverage_factor *
                                         photon_count_per_bin;
                new_errors(bin_index) += statistical_weights *
                                         bin_coverage_factor *
                                         std::pow(photon_count_per_bin, 2);
            }
        }
    }
}

} // namespace aare
