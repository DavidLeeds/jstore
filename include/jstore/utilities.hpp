/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <functional>
#include <string_view>

#include <fmt/core.h>

namespace jstore {

/*
 * Error function type.
 */
using error_func = std::function<void(std::string_view)>;

/*
 * If valid, invoke `func` with a formatted error message.
 */
template <typename ErrorFunc, typename ...FmtArgs>
void handle_error(ErrorFunc &&func, fmt::format_string<FmtArgs...> fmt, FmtArgs &&...args)
{
    if (func) {
        func(fmt::vformat(fmt.get(), fmt::make_format_args(args...)));
    }
}

/*
 * Returns a compile-time generated string_view containing a human-readable
 * representation of type T. Unlike using typeid(), this function does not
 * depend on RTTI. To accomplish this, we mildly abuse the __PRETTY_FUNCTION__
 * macro defined by gcc and clang.
 */
template <typename T>
[[nodiscard]] constexpr std::string_view typestr()
{
    /*
     * Expected format:
     * "constexpr std::string_view jstore::typestr() [with T = ???; other types...? ]"
     */
    constexpr std::string_view sv { __PRETTY_FUNCTION__ };
    constexpr std::string_view prefix { "T = " };
    constexpr std::string_view suffixes { ";]" };

    constexpr auto start = sv.find(prefix);
    static_assert(start != std::string_view::npos);

    constexpr auto pos = start + prefix.size();

    constexpr auto end = sv.find_first_of(suffixes, pos);
    static_assert(end != std::string_view::npos);

    return sv.substr(pos, end - pos);
}

} /* namespace jstore */
