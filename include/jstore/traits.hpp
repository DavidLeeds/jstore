/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <array>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <visit_struct/visit_struct.hpp>

namespace jstore::traits {

template <typename T> struct is_array                                   : std::false_type {};
template <typename ...Ts> struct is_array<std::array<Ts...>>            : std::true_type {};
template <typename ...Ts> struct is_array<std::vector<Ts...>>           : std::true_type {};
template <typename ...Ts> struct is_array<std::list<Ts...>>             : std::true_type {};
template <typename ...Ts> struct is_array<std::deque<Ts...>>            : std::true_type {};
template <typename ...Ts> struct is_array<std::set<Ts...>>              : std::true_type {};
template <typename ...Ts> struct is_array<std::unordered_set<Ts...>>    : std::true_type {};
template <typename T> using is_array_t = typename is_array<T>::type;
template <typename T> inline constexpr bool is_array_v = is_array<T>::value;

template <typename T> struct is_map                                     : std::false_type {};
template <typename ...Ts> struct is_map<std::map<Ts...>>                : std::true_type {};
template <typename ...Ts> struct is_map<std::unordered_map<Ts...>>      : std::true_type {};
template <typename T> using is_map_t = typename is_map<T>::type;
template <typename T> inline constexpr bool is_map_v = is_map<T>::value;

template <typename T>
struct is_container : std::disjunction<is_array<T>, is_map<T>, visit_struct::traits::is_visitable<T>> {};
template <typename T> using is_container_t = typename is_container<T>::type;
template <typename T> inline constexpr bool is_container_v = is_container<T>::value;

template <typename T>
concept array = is_array_v<std::decay_t<T>>;

template <typename T>
concept map = is_map_v<std::decay_t<T>>;

template <typename T>
concept convertible_map = map<T> &&
        std::is_convertible_v<typename T::key_type, nlohmann::json::object_t::key_type>;

template <typename T>
concept not_convertible_map = map<T> &&
        !std::is_convertible_v<typename T::key_type, nlohmann::json::object_t::key_type>;

template <typename T>
concept visitable = visit_struct::traits::is_visitable<std::decay_t<T>>::value;

template <typename T>
concept container = is_container_v<std::decay_t<T>>;

template <typename T>
concept not_container = !is_container_v<std::decay_t<T>>;

} /* namespace jstore::traits */
