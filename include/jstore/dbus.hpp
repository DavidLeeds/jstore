/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <functional>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <sdbus-c++/sdbus-c++.h>
#include <visit_struct/visit_struct.hpp>

#include <jstore/for_each.hpp>
#include <jstore/path_to.hpp>
#include <jstore/serialization.hpp>
#include <jstore/traits.hpp>
#include <jstore/utilities.hpp>
#include <jstore/visit_path.hpp>

namespace jstore {

enum class access_type {
    READ,
    WRITE
};

inline const sdbus::InterfaceName DBUS_INTERFACE{"io.davidleeds.JStore"};

template <typename Root>
class dbus {
public:
    using root_type = Root;
    using filter_func = std::function<bool(const std::string &, access_type)>;
    using set_func = std::function<void(const std::string &)>;

    /*
     * Add the io.davidleeds.JStore D-Bus interface to the supplied object.
     * This interface reflects the org.freedesktop.DBus.Properties interface,
     * but supports flexible path strings to access dynamically changing tree content.
     */
    dbus(root_type &root, sdbus::IObject &object, filter_func filter = {}) :
        root_(root),
        object_(object),
        filter_(std::move(filter))
    {
        std::vector<sdbus::VTableItem> vtable;

        vtable.emplace_back(
                sdbus::registerMethod("Get")
                .implementedAs([this](const std::string &path) {
                    std::string val;

                    bool found = jstore::visit_path(root_, path, [this, &path, &val](const auto &member) {
                        if (filter_ && !filter_(path, access_type::READ)) {
                            throw sdbus::createError(EACCES, "no read access");
                        }

                        json j;
                        serialize(j, member, false);
                        val = j.dump();
                    });

                    if (!found) {
                        throw sdbus::createError(ENOENT, "unknown item");
                    }

                    return val;
                })
                .withInputParamNames("Path")
                .withOutputParamNames("Value")
        );

        vtable.emplace_back(
                sdbus::registerMethod("GetAll")
                .implementedAs([this]() {
                    values_builder builder{filter_};

                    builder.add("", root_);

                    return builder.values;
                })
                .withOutputParamNames("Values")
        );

        vtable.emplace_back(
                sdbus::registerMethod("Set")
                .implementedAs([this](const std::string &path, const std::string &val) {
                    bool found = jstore::visit_path(root_, path, [this, &path, &val](auto &member) {
                        if (filter_ && !filter_(path, access_type::WRITE)) {
                            throw sdbus::createError(EACCES, "no write access");
                        }

                        json j;

                        try {
                            j = json::parse(val);
                        } catch (const std::exception &e) {
                            throw sdbus::createError(EINVAL,
                                    fmt::format("JSON parse error: {}", e.what()));
                        }

                        std::string error_msg;
                        auto on_error = [&error_msg](std::string &&msg) {
                            error_msg = std::move(msg);
                        };

                        if (!deserialize(j, member, on_error)) {
                            throw sdbus::createError(EINVAL, error_msg);
                        }

                        /* Notify application about remote set */
                        if (on_set_) {
                            on_set_(path);
                        }
                    }, true /* insert_keys */);

                    if (!found) {
                        throw sdbus::createError(ENOENT, "unknown item");
                    }
                })
                .withInputParamNames("Path", "Value")
        );

        vtable.emplace_back(
                sdbus::registerSignal("ValuesChanged")
                .withParameters<std::map<std::string, std::string>>("Values")
        );

        vtable_slot_ = object.addVTable(DBUS_INTERFACE, std::move(vtable), sdbus::return_slot);
    }

    /*
     * Emit a ValuesChanged signal with entries for each node in the
     * argument list. If the specified node is a container with individually
     * accessible children, an entry will be added for each child node.
     */
    template <typename ...Nodes>
    void emit_values_changed(const Nodes &...nodes)
    {
        static_assert(sizeof...(nodes) > 0);

        values_builder builder{filter_};

        /* Lambda to find the specified tree node and populate it in the values map */
        auto add_node = [this, &builder](const auto &node) {
            std::optional<std::string> path = path_to(root_, node);

            if (!path.has_value()) {
                throw std::invalid_argument(fmt::format("{} node is not in the tree",
                        typestr<std::decay_t<decltype(node)>>()));
            }

            builder.add(path.value(), node);
        };

        /* Add entries for each node in the parameter pack */
        ((add_node(nodes)), ...);

        if (!builder.values.empty()) {
            object_.emitSignal("ValuesChanged")
                    .onInterface(DBUS_INTERFACE)
                    .withArguments(builder.values);
        }
    }

    void on_set(set_func callback)
    {
        on_set_ = std::move(callback);
    }

private:
    /*
     * Builder object that selects which tree elements to export to keep
     * observers' caches consistent.
     */
    struct values_builder {
        filter_func &filter_;
        std::map<std::string, std::string> values;

        template <traits::array T>
        void add(const std::string &path, const T &container)
        {
            /*
             * Array elements are identified using an index in the path.
             * Because not all types represented by array types support
             * random indexed access (e.g. std::set), cached copies
             * of array types owned by observers cannot necessarily be
             * kept consistent if individual elements are pushed. Thus,
             * we push the entire container on change.
             */
            add_internal(path, container);
        }

        template <traits::map T>
        void add(const std::string &path, const T &container)
        {
            if (container.empty()) {
                /* If empty, push an empty object to invalidate any cached elements */
                add_internal(path, container);
            } else {
                /* Recurse into map values, as these can be individually applied */
                for (auto &[key, value] : container) {
                    add(fmt::format("{}{}{}", path, path.empty() ? "" : "/", key), value);
                }
            }
        }

        template <traits::visitable T>
        void add(const std::string &path, const T &container)
        {
            /* Visitable structs have fixed content, so always push individual members */
            visit_struct::for_each(container, [this, &path](const char *key, auto &member) {
                add(fmt::format("{}{}{}", path, path.empty() ? "" : "/", key), member);
            });
        }

        template <traits::leaf T>
        void add(const std::string &path, const T &value)
        {
            add_internal(path, value);
        }

    private:
        template <typename T>
        void add_internal(const std::string &path, const T &member)
        {
            if (filter_ && !filter_(path, access_type::READ)) {
                return;
            }

            json j;
            serialize(j, member);
            values.emplace(path, j.dump());
        }
    }; /* values_builder */

    root_type &root_;
    sdbus::IObject &object_;
    sdbus::Slot vtable_slot_;
    filter_func filter_;
    set_func on_set_;
};

} /* namespace jstore */
