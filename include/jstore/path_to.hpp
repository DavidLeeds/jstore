/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <optional>
#include <string>

#include <jstore/for_each.hpp>

namespace jstore {

template <typename ContainerType, typename NodeType>
std::optional<std::string> path_to(const ContainerType &container, const NodeType &node)
{
    std::optional<std::string> path_to_node;

    jstore::for_each<ALL>(container, [&node, &path_to_node](const std::string &path, const auto &member) {
        using node_type = std::decay_t<decltype(node)>;
        using member_type = std::decay_t<decltype(member)>;

        if constexpr (std::is_same_v<node_type, member_type>) {
            if (std::addressof(node) == std::addressof(member)) {
                path_to_node = path;
            }
        }
    });

    return path_to_node;
}

} /* namespace jstore */
