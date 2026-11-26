// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <filesystem>
#include <string>

namespace aare {

/**
 * @brief Read the JSON header of a state file as text.
 * @param fname File to read.
 * @throws std::runtime_error if the file cannot be read, is not a state
 * file, was written in a newer format, or its header is truncated or
 * damaged.
 */
std::string read_state_header(const std::filesystem::path &fname);

/**
 * @brief Write a state to a file.
 * @tparam State FastPedestalState, ClusterFinderState,
 * ClusterFinderMTState or VarClusterFinderState; any other type fails to
 * link.
 * @param state State to write, as returned by state() of its class. Every
 * dtype in it must be set and n_sigma finite.
 * @param fname File to write. An existing file is replaced.
 * @throws std::runtime_error if the state cannot be saved or the file
 * cannot be written.
 */
template <typename State>
void save_state(const State &state, const std::filesystem::path &fname);

/**
 * @brief Read a state written by save_state().
 * @tparam State Type of the state stored in the file, see save_state().
 * @param fname File to read.
 * @throws std::runtime_error if the file cannot be read, is not a state
 * file, was written in a newer format, holds the state of another type, or
 * is truncated or damaged.
 */
template <typename State> State load_state(const std::filesystem::path &fname);

} // namespace aare
