#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string>
#include <vector>

// function check connected that reads from a file if modules are connected or
// not - which module though?

// function read flatfield store in ff_corr probably correlation field

namespace aare {

using parameters =
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;

struct MythenSpecifications {

    static constexpr int32_t max_modules = 48;
    static constexpr int32_t strips_per_module = 1280;
    static constexpr double pitch = 0.05; // strip width [mm] ?? TODO: not sure
    static constexpr double ttmin = -180.0; // what is this?
    static constexpr float ttmax = 180.0;
    static constexpr float ttstep =
        0.0036; // probably here to calculate bin size, what is this?

    static constexpr double bloffset =
        1.532; // what is this? detector offset relative to what?
};

// number_of_activated_modules
// number_of_channles
// number_of_dimension
// is bad array keeping track of bad channels!!
class AngleCalibration {

  public:
    AngleCalibration() {
        centers.reserve(MythenSpecifications::max_modules);
        conversions.reserve(MythenSpecifications::max_modules);
        offsets.reserve(MythenSpecifications::max_modules);
    }

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

    /** calculated the strip width expressed as angle [degrees]
     * @param strip_index gloabl strip index of detector
     */
    double angular_strip_width(const size_t strip_index);

    /** converts global strip index to local strip index of that module */
    size_t
    global_to_local_strip_index_conversion(const size_t global_strip_index) {
        const size_t module_index =
            global_strip_index / MythenSpecifications::strips_per_module;
        // local strip index in module
        size_t local_strip_index =
            global_strip_index -
            module_index * MythenSpecifications::strips_per_module;
        // switch if indexing is in clock-wise direction
        local_strip_index =
            signbit(conversions[module_index])
                ? MythenSpecifications::strips_per_module - local_strip_index
                : local_strip_index;

        return local_strip_index;
    }

  protected:
    // TODO: Design maybe have a struct with three vectors, store all three
    // sets of parameters as member variables

    // TODO: check if interpretation and units are correct
    // historical DG parameters
    std::vector<double> centers; // orthogonal projection of sample onto
                                 // detector (given in strip number) [mm]
                                 // D/pitch
    std::vector<double>
        conversions; // pitch/(normal distance from sample to detector (R)) [mm]
                     // //used for easy conversion
    std::vector<double>
        offsets; // position of strip zero relative to sample [degrees] phi -
                 // 180/pi*D/R TODO: expected an arcsin(D/R)?
};

// read hdf5 files - > do they store the histogram? what angles do they store?

// TODO what kind of file does it need to support? - probably code a csv parser
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
        centers[module_index] * MythenSpecifications::pitch;
    const double module_center_distance =
        MythenSpecifications::pitch / std::abs(conversions[module_index]);
    const double angle =
        offsets[module_index] +
        180.0 / M_PI * centers[module_index] *
            std::abs(conversions[module_index]); // TODO: maybe add function
                                                 // rad_to_deg

    return std::make_tuple(normal_distance, module_center_distance, angle);
}

/*
parameters
AngleCalibration::convert_to_BC_parameters() {}
*/

double AngleCalibration::diffraction_angle_from_EE_parameters(
    const double normal_distance, const double module_center_distance,
    const double angle, const size_t strip_index) {

    return angle - 180.0 / M_PI *
                       atan((module_center_distance -
                             MythenSpecifications::pitch * strip_index) /
                            normal_distance); // TODO: why is it minus
                                              // is it defined counter
                                              // clockwise? thought
                                              // should have a flipped
                                              // sign
}

double AngleCalibration::angular_strip_width(const size_t strip_index) {

    const size_t module_index =
        strip_index / MythenSpecifications::strips_per_module;

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

/*
void AngleCalibration::Calibrate() {

    // iterates over each strip
    // skips the one which are not enabled or have a bad channel
    // get module index
    // get index of strip in module - make sure to use function correctly based
    // on sign

    // then I read some images - probably the histogram - h5 file - need a
    // reader need to read the filesystem!! -

    // we modify it with ff_corr computed by flatfield why?


}
*/

} // namespace aare
