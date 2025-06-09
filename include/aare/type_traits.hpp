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

} // namsespace aare