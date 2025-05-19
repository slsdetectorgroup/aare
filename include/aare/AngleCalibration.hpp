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

// TODO: can i have a static struct, constexpr?
struct MythenSpecifications {

    static constexpr int32_t max_modules = 48;
    static constexpr int32_t channels_per_module = 1280;
    static constexpr double pitch =
        0.05; // what is this pitch pitch is up e.g. rotation aroung y axis
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

    void read_initial_calibration_from_file(const std::string &filename);

  protected:
    std::vector<double> centers;
    std::vector<double> conversions;
    std::vector<double> offsets;
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

    // angle_sign = signbit(conversion);
    // signed_angles = std::abs(conversion);
    // inverse_angles = 1./signed_angles
    // store [centers, signed_conversions, offsets]
    // dont know what conversion and offset is - dont know what calculations one
    // does there the errors are actually not needed
}

} // namespace aare