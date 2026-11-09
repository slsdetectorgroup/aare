// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FastPedestal.hpp"

#include <filesystem>
#include <string>

namespace aare {

/**
 * @brief Read the JSON header of a state file as text.
 *
 * The header tells what the file holds without reading the arrays: the
 * format version, the aare version that wrote it, the type of the state and
 * the state itself with its scalars, where each array appears as its
 * description (dtype, shape and offset) in place of its data. The StateFile
 * page of the documentation describes the fields.
 * @param fname File to read.
 * @throws std::runtime_error if the file cannot be read, is not a state
 * file, was written in a newer format, or its header is truncated or
 * damaged.
 */
std::string read_state_header(const std::filesystem::path &fname);

/**
 * @brief Write a state to a file.
 *
 * A state file holds a JSON header followed by the arrays of the state as
 * raw little-endian data; the StateFile page of the documentation describes
 * the layout. The file is written under a temporary name next to fname and
 * then renamed, so that fname is replaced only by a complete file.
 * @param state State to write, as returned by FastPedestal::state(). Its
 * dtype must be set.
 * @param fname File to write. An existing file is replaced.
 * @throws std::runtime_error if the state has no dtype or the file cannot be
 * written.
 */
void save_state(const FastPedestalState &state,
                const std::filesystem::path &fname);

/**
 * @brief Read a state written by save_state().
 *
 * Fields of the header that this version does not know are ignored. What is
 * read is not checked for consistency beyond its types and array ranks; that
 * is done where the state is used, for example by FastPedestal::set_state().
 * @tparam State Type of the state stored in the file. Supported:
 * FastPedestalState.
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

} // namespace aare
