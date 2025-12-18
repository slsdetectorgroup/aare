// SPDX-License-Identifier: MPL-2.0

/*
 *The file to_string.hpp contains conversion to and from string for various aare
 * types. It is intentionally not a part of the public API.
 * Only include the conversion that are actually needed!!!
 */

#include "aare/defs.hpp" //enums

namespace aare {

// if T has a constructor that takes a string, lets use it.
template <class T> T string_to(const std::string &arg) { return T{arg}; }

/**
 * @brief Convert a string to DetectorType
 * @param name string representation of the DetectorType
 * @return DetectorType
 * @throw runtime_error if the string does not match any DetectorType
 */
template <> DetectorType string_to(const std::string &arg);

/**
 * @brief Convert a string to TimingMode
 * @param mode string representation of the TimingMode
 * @return TimingMode
 * @throw runtime_error if the string does not match any TimingMode
 */
template <> TimingMode string_to(const std::string &arg);

/**
 * @brief Convert a string to FrameDiscardPolicy
 * @param mode string representation of the FrameDiscardPolicy
 * @return FrameDiscardPolicy
 * @throw runtime_error if the string does not match any FrameDiscardPolicy
 */
template <> FrameDiscardPolicy string_to(const std::string &arg);

/**
 * @brief Convert a string to a DACIndex
 * @param arg string representation of the dacIndex
 * @return DACIndex
 * @throw invalid argument error if the string does not match any DACIndex
 */
template <> DACIndex string_to(const std::string &arg);

} // namespace aare