/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <charconv>
#include <concepts>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <visit_struct/visit_struct.hpp>

#include <jstore/serialization.hpp>
#include <jstore/traits.hpp>
#include <jstore/utilities.hpp>

namespace jstore {

/*
 * Split the first slash-delimited path segment from the remainder of the path.
 */
inline std::pair<std::string_view, std::string_view> split_path(std::string_view path)
{
    auto slash_pos = path.find_first_of('/');

    return {
        path.substr(0, slash_pos),
        (slash_pos < path.size() - 1) ? path.substr(slash_pos + 1) : std::string_view{}
    };
}


/*
 * Visit path forward declarations
 */
template <traits::array T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys = false, const error_func &on_error = {});
template <traits::map T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys = false, const error_func &on_error = {});
template <traits::visitable T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys = false, const error_func &on_error = {});
template <traits::not_container T, typename Func>
bool visit_path(T &value, std::string_view path, const Func &func, bool insert_keys = false, const error_func &on_error = {});


/*
 * Visit path for array-like types.
 */
template <traits::array T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys, const error_func &on_error)
{
    /* End traversal on empty path */
    if (path.empty()) {
        func(container);
        return true;
    }

    auto [index_str, child_path] = split_path(path);
    size_t index = 0;
    auto [ptr, ec] = std::from_chars(index_str.begin(), index_str.end(), index, 10);
    /* Parse error */
    if (ec != std::errc{}) {
        handle_error(on_error, "malformed array index in path segment: '{}' ({})", index_str, std::make_error_code(ec).message());
        return false;
    }
    /* Unparsed characters */
    if (ptr != index_str.data() + index_str.size()) {
        handle_error(on_error, "malformed array index in path segment: '{}'", index_str);
        return false;
    }

    /* Index is out of bounds */
    if (index >= container.size()) {
        return false;
    }

    /* Specified array element is present: recurse */
    auto it = next(container.begin(), index);
    return visit_path(*it, child_path, func, insert_keys, on_error);
}

/*
 * Visit path for map-like types.
 */
template <traits::map T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys, const error_func &on_error)
{
    /* End traversal on empty path */
    if (path.empty()) {
        func(container);
        return true;
    }

    using iterator = typename T::iterator;
    using key_type = typename T::key_type;
    auto [key, child_path] = split_path(path);
    iterator it;

    /* Don't support path lookup of empty map keys */
    if (key.empty()) {
        handle_error(on_error, "empty map key not supported");
        return false;
    }

    if constexpr (std::is_convertible_v<std::string, key_type>) {
        /* Key is compatible with string */
        if (insert_keys) {
            it = container.try_emplace(std::string{key}).first;
        } else {
            it = container.find(std::string{key});
        }
    } else {
        /* Key is not string-like; must parse */
        std::istringstream ss{std::string{key}};
        key_type key_value{};

        ss >> key_value;

        if (ss.fail() || !ss.eof()) {
            handle_error(on_error, "malformed map key in path segment: '{}'", key);
            return false;
        }

        if (insert_keys) {
            it = container.try_emplace(key_value).first;
        } else {
            it = container.find(key_value);
        }
    }

    /* Map key is not present and was not inserted */
    if (it == container.end()) {
        return false;
    }

    /* Specified map element is present: recurse */
    return visit_path(it->second, child_path, func, insert_keys, on_error);
}

/*
 * Visit path for visitable structs.
 */
template <traits::visitable T, typename Func>
bool visit_path(T &container, std::string_view path, const Func &func, bool insert_keys, const error_func &on_error)
{
    /* End traversal on empty path */
    if (path.empty()) {
        func(container);
        return true;
    }

    auto [member, child_path] = split_path(path);
    bool found = false;
    bool result = false;

    /* If specified struct member is present, recurse */
    visit_struct::for_each(container, [&](const char *key, auto &value) {
        if (!found && member == key) {
            found = true;
            result = visit_path(value, child_path, func, insert_keys, on_error);
        }
    });

    return result;
}

/*
 * Visit path for non-container types.
 */
template <traits::not_container T, typename Func>
bool visit_path(T &value, std::string_view path, const Func &func, bool insert_keys, const error_func &on_error)
{
    if (!path.empty()) {
        handle_error(on_error, "unreachable path segment: '{}' ({} is not a container)", path, typestr<T>());
        return false;
    }

    func(value);
    return true;
}

} /* namespace jstore */
