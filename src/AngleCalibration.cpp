#include "aare/AngleCalibration.hpp"

namespace aare {

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

size_t AngleCalibration::global_to_local_strip_index_conversion(
    const size_t global_strip_index) {
    const size_t module_index =
        global_strip_index / MythenDetectorSpecifications::strips_per_module();
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
