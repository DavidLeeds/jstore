/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <concepts>
#include <utility>

#include <nlohmann/json.hpp>
#include <visit_struct/visit_struct.hpp>

#include <jstore/traits.hpp>
#include <jstore/utilities.hpp>

namespace jstore {

using json = nlohmann::json;

/*
 * Serializer forward declarations.
 */
template <traits::array T>
bool serialize(json &j, const T &container, bool omit_defaults = false, const error_func &on_error = {});
template <traits::convertible_map T>
bool serialize(json &j, const T &container, bool omit_defaults = false, const error_func &on_error = {});
template <traits::not_convertible_map T>
bool serialize(json &j, const T &container, bool omit_defaults = false, const error_func &on_error = {});
template <traits::visitable T>
bool serialize(json &j, const T &container, bool omit_defaults = false, const error_func &on_error = {});
template <traits::leaf T>
bool serialize(json &j, const T &value, bool omit_defaults = false, const error_func &on_error = {});


/*
 * Pack an array-like type.
 */
template <traits::array T>
bool serialize(json &j, const T &container, bool omit_defaults, const error_func &on_error)
{
    j = json::array();

    for (auto &value : container) {
        json v;

        serialize(v, value, omit_defaults, on_error);
        j.push_back(std::move(v));
    }

    return !j.empty();
}

/*
 * Pack a map-like type with string keys.
 */
template <traits::convertible_map T>
bool serialize(json &j, const T &container, bool omit_defaults, const error_func &on_error)
{
    j = json::object();

    for (auto &[key, value] : container) {
        json v;

        serialize(v, value, omit_defaults, on_error);
        j[key] = std::move(v);
    }

    return !j.empty();
}

/*
 * Pack a map-like type with non-string keys.
 */
template <traits::not_convertible_map T>
bool serialize(json &j, const T &container, bool omit_defaults, const error_func &on_error)
{
    j = json::array();

    for (auto &[key, value] : container) {
        json k, v;

        if (serialize(k, key, false, on_error)) {
            serialize(v, value, omit_defaults, on_error);
            j.push_back({ std::move(k), std::move(v) });
        }
    }

    return !j.empty();
}

/*
 * Pack a visitable struct.
 *
 * Since there are a fixed number of elements and the default values are known,
 * default values are not serialized.
 */
template <traits::visitable T>
bool serialize(json &j, const T &container, bool omit_defaults, const error_func &on_error)
{
    /*
     * Note: `defaults` is static to avoid default constructing it on each serialize() call.
     * To avoid bugs, structs' initial values must not change.
     */
    static thread_local const T defaults{};

    /* To avoid overwriting unknown keys, only clear j if it is not the expected type */
    if (!j.is_object()) {
        j = json::object();
    }

    visit_struct::for_each(container, defaults, [&j, omit_defaults, &on_error](const char *key, const auto &value, const auto &def) {
        bool written = false;

        using member_type = decltype(value);
        static_assert(std::equality_comparable<member_type>, "members of visitable_structs must be equality comparable");

        if (!omit_defaults || value != def) {
            json v;

            if (serialize(v, value, omit_defaults, on_error)) {
                j[key] = std::move(v);
                written = true;
            }
        }

        if (!written) {
            /* Clear existing element */
            if (auto it = j.find(key); it != j.end()) {
                j.erase(it);
            }
        }
    });

    return !j.empty();
}

/*
 * Pack a leaf node.
 *
 * Note that `void to_json(json &, const T &)` MUST be defined for type T.
 */
template <traits::leaf T>
bool serialize(json &j, const T &value, bool omit_defaults, const error_func &on_error)
{
    try {
        j = value;
    } catch (const std::exception &e) {
        j.clear();
        handle_error(on_error, "failed to serialize {}: {}", typestr<T>(), e.what());
        return false;
    }

    return true;
}


/*
 * Deserializer forward declarations.
 */
template <traits::array T>
bool deserialize(const json &j, T &container, const error_func &on_error = {});
template <traits::convertible_map T>
bool deserialize(const json &j, T &container, const error_func &on_error = {});
template <traits::not_convertible_map T>
bool deserialize(const json &j, T &container, const error_func &on_error = {});
template <traits::visitable T>
bool deserialize(const json &j, T &container, const error_func &on_error = {});
template <traits::leaf T>
bool deserialize(const json &j, T &value, const error_func &on_error = {});


/*
 * Unpack an array-like type.
 */
template <traits::array T>
bool deserialize(const json &j, T &container, const error_func &on_error)
{
    if (!j.is_array()) {
        handle_error(on_error, "failed to deserialize '{}': JSON type is {}", typestr<T>(), j.type_name());
        return false;
    }

    container.clear();

    for (auto &v : j) {
        typename T::value_type value {};

        deserialize(v, value, on_error);
        container.insert(container.end(), std::move(value));
    }

    return true;
}

/*
 * Unpack a map-like type with string keys.
 */
template <traits::convertible_map T>
bool deserialize(const json &j, T &container, const error_func &on_error)
{
    if (!j.is_object()) {
        handle_error(on_error, "failed to deserialize '{}': JSON type is {}", typestr<T>(), j.type_name());
        return false;
    }

    container.clear();

    /* Map serialization with string keys */
    for (auto &[k, v] : j.items()) {
        typename T::mapped_type value {};

        deserialize(v, value, on_error);
        container.emplace(k, std::move(value));
    }

    return true;
}

/*
 * Unpack a map-like type with non-string keys.
 */
template <traits::not_convertible_map T>
bool deserialize(const json &j, T &container, const error_func &on_error)
{
    if (!j.is_array()) {
        handle_error(on_error, "failed to deserialize '{}': JSON type is {}", typestr<T>(), j.type_name());
        return false;
    }

    container.clear();

    /* Map serialization with non-string keys */
    for (auto &v : j) {
        if (!v.is_array() || v.size() != 2) {
            handle_error(on_error, "ignoring unexpected map entry: {}[{}]", j.type_name(), j.size());
            continue;
        }

        typename T::key_type key;

        if (deserialize(v[0], key, on_error)) {
            typename T::mapped_type value {};

            deserialize(v[1], value, on_error);
            container.emplace(std::move(key), std::move(value));
        }
    }

    return true;
}

/*
 * Unpack a visitable struct.
 */
template <traits::visitable T>
bool deserialize(const json &j, T &container, const error_func &on_error)
{
    if (!j.is_object()) {
        handle_error(on_error, "failed to deserialize '{}': JSON type is {}", typestr<T>(), j.type_name());
        return false;
    }

    static const T defaults{};

    visit_struct::for_each(container, defaults, [&j, &on_error](const char *key, auto &value, const auto &def) {
        if (j.contains(key)) {
            deserialize(j.at(key), value, on_error);
        } else if (value != def) {
            /* Restore default value for unsaved member */
            value = def;
        }
    });

    return true;
}

/*
 * Unpack a leaf node.
 *
 * Note that `void from_json(const json &, T &)` MUST be defined for type T.
 */
template <traits::leaf T>
bool deserialize(const json &j, T &value, const error_func &on_error)
{
    try {
        j.get_to(value);
    } catch (const std::exception &e) {
        handle_error(on_error, "failed to deserialize '{}': {}", typestr<T>(), e.what());
        return false;
    }

    return true;
}

} /* namespace jstore */
