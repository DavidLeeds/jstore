/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <string>

#include <visit_struct/visit_struct.hpp>
#include <fmt/core.h>

#include <jstore/traits.hpp>

namespace jstore {

/*
 * Tree traversal types (one-hot encoded)
 */
enum traversal : uint8_t {
    LEAF        = 0x01,
    NON_LEAF    = 0x02,
    ALL         = LEAF | NON_LEAF
};

/*
 * Tree traversal forward declarations.
 */
template <traversal Traversal, traits::array T, typename Func>
void for_each(T &container, const std::string &path, const Func &func);
template <traversal Traversal, traits::map T, typename Func>
void for_each(T &container, const std::string &path, const Func &func);
template <traversal Traversal, traits::visitable T, typename Func>
void for_each(T &container, const std::string &path, const Func &func);
template <traversal Traversal, traits::not_container T, typename Func>
void for_each(T &container, const std::string &path, const Func &func);


/*
 * Tree traversal for array-like types.
 */
template <traversal Traversal, traits::array T, typename Func>
void for_each(T &container, const std::string &path, const Func &func)
{
    if constexpr (Traversal & NON_LEAF) {
        func(path, container);
    }

    size_t index = 0;

    for (auto &value : container) {
        for_each<Traversal>(value, fmt::format("{}{}{}", path, path.empty() ? "" : "/", index), func);
        ++index;
    }
}

/*
 * Tree traversal for map-like types.
 */
template <traversal Traversal, traits::map T, typename Func>
void for_each(T &container, const std::string &path, const Func &func)
{
    if constexpr (Traversal & NON_LEAF) {
        func(path, container);
    }

    for (auto &[key, value] : container) {
        for_each<Traversal>(value, fmt::format("{}{}{}", path, path.empty() ? "" : "/", key), func);
    }
}

/*
 * Tree traversal for visitable structs.
 */
template <traversal Traversal, traits::visitable T, typename Func>
void for_each(T &container, const std::string &path, const Func &func)
{
    if constexpr (Traversal & NON_LEAF) {
        func(path, container);
    }

    visit_struct::for_each(container, [&path, &func](const char *key, auto &value) {
        for_each<Traversal>(value, fmt::format("{}{}{}", path, path.empty() ? "" : "/", key), func);
    });
}

/*
 * Leaf node handler.
 */
template <traversal Traversal, traits::not_container T, typename Func>
void for_each(T &value, const std::string &path, const Func &func)
{
    if constexpr (Traversal & LEAF) {
        func(path, value);
    }
}

/*
 * Tree traversal for top-level nodes.
 */
template <traversal Traversal, typename T, typename Func>
void for_each(T &container, const Func &func)
{
    for_each<Traversal>(container, std::string{}, func);
}

} /* namespace jstore */
