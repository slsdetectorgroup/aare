// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <filesystem>
#include <string>

namespace aare {

// Defined in the header of their class, which the caller of save_state() and
// load_state() includes anyway
struct FastPedestalState;
struct ClusterFinderState;

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
 * @param state State to write, as returned by FastPedestal::state(). Its
 * dtype must be set.
 * @param fname File to write. An existing file is replaced.
 * @throws std::runtime_error if the state has no dtype or the file cannot be
 * written.
 */
void save_state(const FastPedestalState &state,
                const std::filesystem::path &fname);

/**
 * @brief Write the state of a cluster finder to a file.
 * @param state State to write, as returned by ClusterFinder::state(). Its
 * cluster_dtype and the dtype of its pedestal must be set and its n_sigma
 * finite.
 * @param fname File to write. An existing file is replaced.
 * @throws std::runtime_error if a dtype is not set, n_sigma is not finite
 * or the file cannot be written.
 */
void save_state(const ClusterFinderState &state,
                const std::filesystem::path &fname);

/**
 * @brief Read a state written by save_state().
 * @tparam State Type of the state stored in the file. Supported:
 * FastPedestalState and ClusterFinderState.
 * @param fname File to read.
 * @throws std::runtime_error if the file cannot be read, is not a state
 * file, was written in a newer format, holds the state of another type, or
 * is truncated or damaged.
 */
template <typename State>
State load_state(const std::filesystem::path &fname) = delete;

template <>
FastPedestalState
load_state<FastPedestalState>(const std::filesystem::path &fname);

template <>
ClusterFinderState
load_state<ClusterFinderState>(const std::filesystem::path &fname);

} // namespace aare
