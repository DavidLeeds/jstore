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

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <visit_struct/visit_struct.hpp>

#include <jstore/for_each.hpp>
#include <jstore/serialization.hpp>
#include <jstore/stdio_fstream.hpp>
#include <jstore/traits.hpp>
#include <jstore/utilities.hpp>
#include <jstore/visit_path.hpp>

#if JSTORE_SDBUSCPP
#include <jstore/dbus.hpp>
#endif

namespace jstore {

using json = nlohmann::json;

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
    using dbus_type = jstore::dbus<Root>;

    /*
     * Add the io.davidleeds.JStore D-Bus interface to the supplied object.
     * This interface reflects the org.freedesktop.DBus.Properties interface,
     * but supports flexible path strings to access dynamically changing tree content.
     */
    void register_dbus(sdbus::IObject &object, dbus_type::filter_func filter = {})
    {
        dbus_ = std::make_unique<dbus_type>(root_, object, std::move(filter));
    }

    /*
     * Disable D-Bus access.
     */
    void unregister_dbus()
    {
        dbus_.reset();
    }

    dbus_type &dbus()
    {
        if (!dbus_) {
            throw std::runtime_error("D-Bus interface not registered");
        }

        return *dbus_;
    }

private:
    std::unique_ptr<dbus_type> dbus_;
#endif /* JSTORE_SDBUSCPP */

private:
    std::filesystem::path path_;
    error_func on_error_;
    root_type root_;
};

} /* namespace jstore */
