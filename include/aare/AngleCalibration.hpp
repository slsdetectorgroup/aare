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

// class variables:

namespace aare {

using parameters =
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;

// TODO: can i have a static struct, constexpr?
struct MythenSpecifications {

    static constexpr int32_t max_modules = 48;
    static constexpr int32_t channels_per_module = 1280;
    static constexpr double pitch = 0.05; // strip width [mm] ?? TODO: not sure
    static constexpr double ttmin = -180.0; // what is this the angle
    static constexpr float ttmax = 180.0;
    static constexpr float ttstep =
        0.0036; // probably here to calculate bin size
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

    /** converts DG parameters to easy BC parameters e.g. best computing
     * parameters */
    parameters convert_to_BC_parameters();

  protected:
    // TODO: Design maybe have a struct with three vectors, store all three sets
    // of parameters

    // TODO: check if interpretation and units are correct
    // historical DG parameters
    std::vector<double>
        centers; // orthogonal projection of sample onto detector (given in
                 // strip number) [mm] D/pitch
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
        std::cerr << "Error: " << e.what()
                  << std::endl; // TODO: replace with log
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
        normal_distances[i] = centers[i] * MythenSpecifications::pitch;
        module_center_distances[i] =
            MythenSpecifications::pitch / std::abs(conversions[i]);
        angles[i] =
            offsets[i] + 180.0 / M_PI * centers[i] * std::abs(conversions[i]);
    }
    // TODO: maybe add function rad_to_deg

    return std::make_tuple(normal_distances, module_center_distances, angles);
}
/*
parameters
AngleCalibration::convert_to_BC_parameters() {}
*/

} // namespace aare
