/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <type_traits>

#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <visit_struct/visit_struct.hpp>

#if JSTORE_SDBUSCPP
#include <sdbus-c++/sdbus-c++.h>
#endif

#include <jstore/for_each.hpp>
#include <jstore/serialization.hpp>
#include <jstore/stdio_fstream.hpp>
#include <jstore/traits.hpp>
#include <jstore/utilities.hpp>
#include <jstore/visit_path.hpp>

namespace jstore {

using json = nlohmann::json;

#if JSTORE_SDBUSCPP
    inline const std::string DBUS_INTERFACE = "io.davidleeds.JStore";
#endif


/*
 * JSON Storage wrapper public API
 */

template <typename Root>
class tree final {
public:
    using root_type = Root;

    /*
     * Construct a new JSON Storage object.
     * A storage path must be provided, even if a file does not exist.
     */
    template <typename ...RootArgs>
    explicit tree(const std::filesystem::path &path, error_func on_error = {}, RootArgs &&...args) :
        path_(path.is_relative() ? std::filesystem::absolute(path) : path),
        on_error_(std::move(on_error)),
        root_(forward<RootArgs>(args)...)
    {
        /* Attempt to load existing content (uses defaults on failure) */
        try {
            load();
        } catch (const std::exception &e) {
            handle_error(on_error_, "failed to load {}: {}", path_.string(), e.what());
        }
    }

    /*
     * Load persisted data and overwrite the current in-memory state.
     */
    void load()
    {
        if (!std::filesystem::exists(path_)) {
            return;
        }

        std::ifstream file(path_);
        json in;

        file >> in;

        /* Deserialize tree (nlohmann::json serializer must be available for each value type) */
        deserialize(in, root_, on_error_);
    }

    /*
     * Persist the current in-memory state.
     */
    void save()
    {
        json old;

        if (std::filesystem::exists(path_)) {
            /* Attempt to load existing content, if file is already present */
            try {
                std::ifstream file(path_);

                file >> old;
            } catch (const std::exception &e) {
                handle_error(on_error_, "failed to load {}: {}", path_.string(), e.what());
                old = json{};
            }
        } else {
            /* Ensure parent directory exists */
            std::filesystem::create_directories(path_.parent_path());
        }

        /*
         * Serialization is performed on top of on-disk content to preserve any
         * unknown keys. This enhances compatibility when interacting with files
         * created by different software versions.
         */
        json out = old;

        /* Serialize tree (nlohmann::json serializer must be available for value types) */
        if (!serialize(out, root_, true, on_error_)) {
            /* No serialized content, so remove the file */
            std::filesystem::remove(path_);
            return;
        }

        /* Skip the disk write if content is unchanged */
        if (out == old) {
            return;
        }

        /* Write to temp file */
        std::filesystem::path temp_path = path_.string() + "~";
        stdio_fstream file(temp_path, std::ios_base::out);

        file << out;

        file.fsync();
        file.close();

        /* Atomically overwrite output file */
        std::filesystem::rename(temp_path, path_);
    }

    const std::filesystem::path &path() const
    {
        return path_;
    }

    root_type &operator*()
    {
        return root_;
    }

    const root_type &operator*() const
    {
        return root_;
    }

    root_type *operator->()
    {
        return &root_;
    }

    const root_type *operator->() const
    {
        return &root_;
    }

    root_type &root()
    {
        return root_;
    }

    const root_type &root() const
    {
        return root_;
    }

    /*
     * Invoke the supplied function with the path and value of each node.
     */
    template <typename LeafFunc>
    void for_each(const LeafFunc &func)
    {
        jstore::for_each<LEAF>(root_, func);
    }


#if JSTORE_SDBUSCPP
    /*
     * Add the io.davidleeds.JStore D-Bus interface to the
     * supplied object. This interface reflects the D-Bus Properties
     * interface, but does not have a fixed interface to allow dynamically
     * changing tree content.
     *
     * Note: this function MUST be called prior to invoking
     * sdbus::IObject::finishRegistration().
     */
    void register_dbus(sdbus::IObject &object)
    {
        dbus_object_ptr_ = &object;

        object.registerMethod("Get")
                .onInterface(DBUS_INTERFACE)
                .implementedAs([this](const std::string &path) {
                    std::string val;

                    bool found = jstore::visit_path(root_, path, [this, &val](const auto &member) {
                        json j;
                        serialize(j, member, false, on_error_);
                        val = j.dump();
                    });

                    if (!found) {
                        throw sdbus::createError(ENOENT, "unknown item");
                    }

                    return val;
                })
                .withInputParamNames("Path")
                .withOutputParamNames("Value");

        object.registerMethod("GetAll")
                .onInterface(DBUS_INTERFACE)
                .implementedAs([this]() {
                    values_builder builder;
                    builder.add("", root_);

                    return builder.values;
                })
                .withOutputParamNames("Values");

        object.registerMethod("Set")
                .onInterface(DBUS_INTERFACE)
                .implementedAs([this](const std::string &path, const std::string &val) {
                    bool found = jstore::visit_path(root_, path, [this, &val](auto &member) {
                        json j;

                        try {
                            j = json::parse(val);
                        } catch (const std::exception &e) {
                            throw sdbus::createError(EINVAL,
                                    fmt::format("JSON parse error: {}", e.what()));
                        }

                        if (!deserialize(j, member, on_error_)) {
                            throw sdbus::createError(EINVAL, fmt::format("failed to apply JSON {} to {}",
                                    j.type_name(), typestr<std::decay_t<decltype(member)>>()));
                        }
                    }, true /* insert_keys */);

                    if (!found) {
                        throw sdbus::createError(ENOENT, "unknown item");
                    }

                    try {
                        save();
                    } catch (const std::exception &e) {
                        throw sdbus::createError(EROFS, e.what());
                    }
                })
                .withInputParamNames("Path", "Value");

                object.registerSignal("ValuesChanged")
                        .onInterface(DBUS_INTERFACE)
                        .withParameters<std::map<std::string, std::string>>("Values");
    }

    /*
     * Create a new D-Bus object that implements the io.davidleeds.JStore D-Bus
     * interface. This interface reflects the D-Bus Properties interface, but does
     * not have a fixed interface to allow dynamically
     * changing tree content.
     */
    void register_dbus(sdbus::IConnection &conn, const std::string &object_path)
    {
        dbus_object_storage_ = sdbus::createObject(conn, object_path);
        register_dbus(*dbus_object_storage_);
        dbus_object_storage_->finishRegistration();
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

        if (!dbus_object_ptr_) {
            throw std::runtime_error("D-Bus interface not registered");
        }

        values_builder builder;

        /* Lambda to find the specified tree node and populate it in the values map */
        auto add_node = [this, &builder](const auto &node) {
            bool found = false;

            jstore::for_each<ALL>(root_, [&builder, &node, &found](const std::string &path, const auto &member) {
                using node_type = std::decay_t<decltype(node)>;
                using member_type = std::decay_t<decltype(member)>;

                if constexpr (std::is_same_v<node_type, member_type>) {
                    if (!found && std::addressof(node) == std::addressof(member)) {
                        found = true;
                        builder.add(path, member);
                    }
                }
            });

            if (!found) {
                throw std::invalid_argument(fmt::format("{} node is not in the tree",
                        typestr<std::decay_t<decltype(node)>>()));
            }
        };

        /* Add entries for each node in the parameter pack */
        ((add_node(nodes)), ...);

        if (!builder.values.empty()) {
            dbus_object_ptr_->emitSignal("ValuesChanged")
                    .onInterface(DBUS_INTERFACE)
                    .withArguments(builder.values);
        }
    }

private:
    /*
     * Builder object that selects which tree elements to export to keep
     * observers' caches consistent.
     */
    struct values_builder {
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

        template <traits::not_container T>
        void add(const std::string &path, const T &value)
        {
            add_internal(path, value);
        }

    private:
        template <typename T>
        void add_internal(const std::string &path, const T &member)
        {
            json j;
            serialize(j, member);
            values.emplace(path, j.dump());
        }
    }; /* values_builder */

    std::unique_ptr<sdbus::IObject> dbus_object_storage_;
    sdbus::IObject *dbus_object_ptr_{nullptr};

public:

#endif /* JSTORE_SDBUSCPP */

private:
    std::filesystem::path path_;
    error_func on_error_;
    root_type root_;
};

} /* namespace jstore */
