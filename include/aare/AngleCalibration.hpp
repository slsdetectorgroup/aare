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

#include "FlatField.hpp"
#include "MythenDetectorSpecifications.hpp"
#include "MythenFileReader.hpp"
#include "NDArray.hpp"

namespace aare {

using parameters =
    std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;

class AngleCalibration {

  public:
    AngleCalibration(
        std::shared_ptr<MythenDetectorSpecifications> mythen_detector_,
        std::shared_ptr<FlatField> flat_field_,
        std::shared_ptr<MythenFileReader> mythen_file_reader_);

    /** set the histogram bin width [degrees] */
    void set_histogram_bin_width(double bin_width);

    double get_histogram_bin_width() const;

    ssize_t get_new_num_bins() const;

    /** reads the historical Detector Group (DG) parameters from file **/
    void read_initial_calibration_from_file(const std::string &filename);

    std::vector<double> get_centers() const;
    std::vector<double> get_conversions() const;

    std::vector<double> get_offsets() const;

    NDView<double, 1> get_new_photon_counts() const;

    NDView<double, 1> get_new_statistical_errors() const;

    /** converts DG parameters to easy EE parameters e.g.geometric
     * parameters */
    parameters convert_to_EE_parameters() const;

    std::tuple<double, double, double>
    convert_to_EE_parameters(const size_t module_index) const;

    std::tuple<double, double, double>
    convert_to_EE_parameters(const double center, const double conversion,
                             const double offset) const;

    /** converts DG parameters to BC parameters e.g. best computing
     * parameters */
    parameters convert_to_BC_parameters() const;

    /**
     * calculates new histogram with fixed sized angle bins
     * for several acquisitions at different detector angles for given frame
     * indices
     * @param start_frame_index, end_frame_index gives range of frames
     */
    void
    calculate_fixed_bin_angle_width_histogram(const size_t start_frame_index,
                                              const size_t end_frame_index);

    void write_to_file(const std::string &filename,
                       const bool store_nonzero_bins = false,
                       const std::filesystem::path &filepath =
                           std::filesystem::current_path()) const;

    /** calculates diffraction angle from EE module parameters (used in
     * Beer's Law)
     * @param strip_index local strip index of module
     */
    double diffraction_angle_from_EE_parameters(
        const double module_center_distance, const double normal_distance,
        const double angle, const size_t strip_index,
        const double distance_to_strip = 0) const;

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
        const size_t strip_index, const double distance_to_strip = 0) const;

    /** calculated the strip width expressed as angle [degrees]
     * @param strip_index local strip index of module
     */
    double angular_strip_width_from_DG_parameters(
        const double center, const double conversion, const double offset,
        const size_t local_strip_index) const;

    double angular_strip_width_from_EE_parameters(
        const double module_center_distance, const double normal_distance,
        const double angle, const size_t local_strip_index) const;

  protected:
    /** converts global strip index to local strip index of that module */
    size_t global_to_local_strip_index_conversion(
        const size_t global_strip_index) const;

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
        NDView<double, 1> inverse_nromalized_flatfield) const;

  private:
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

    std::shared_ptr<MythenFileReader>
        mythen_file_reader; // TODO replace by FileInterface ptr
};

} // namespace aare
