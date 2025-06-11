#pragma once

#include <type_traits>

namespace aare {

/**
 * Type trait to check if a template parameter is a std::chrono::duration
 */

template <typename T, typename _ = void>
struct is_duration : std::false_type {};

template <typename... Ts> struct is_duration_helper {};

template <typename T>
struct is_duration<T,
                   typename std::conditional<
                       false,
                       is_duration_helper<typename T::rep, typename T::period,
                                          decltype(std::declval<T>().min()),
                                          decltype(std::declval<T>().max()),
                                          decltype(std::declval<T>().zero())>,
                       void>::type> : public std::true_type {};

/**
 * Type trait to evaluate if template parameter is
 * complying with a standard container
 */
template <typename T, typename _ = void>
struct is_container : std::false_type {};

template <typename... Ts> struct is_container_helper {};

template <typename T>
struct is_container<
    T, typename std::conditional<
           false,
           is_container_helper<
               typename std::remove_reference<T>::type::value_type,
               typename std::remove_reference<T>::type::size_type,
               typename std::remove_reference<T>::type::iterator,
               typename std::remove_reference<T>::type::const_iterator,
               decltype(std::declval<T>().size()),
               decltype(std::declval<T>().begin()),
               decltype(std::declval<T>().end()),
               decltype(std::declval<T>().cbegin()),
               decltype(std::declval<T>().cend()),
               decltype(std::declval<T>().empty())>,
           void>::type> : public std::true_type {};


/**
 * Type trait to evaluate if template parameter is
 * complying with a std::string
 */
template <typename T>
inline constexpr bool is_std_string_v =
    std::is_same_v<std::decay_t<T>, std::string>;

/**
 * Type trait to evaluate if template parameter is
 * complying with std::map
 */
template <typename T>
struct is_map : std::false_type {};

template <typename K, typename V, typename... Args>
struct is_map<std::map<K, V, Args...>> : std::true_type {};

template <typename T>
inline constexpr bool is_map_v = is_map<std::decay_t<T>>::value;

} // namsespace aare