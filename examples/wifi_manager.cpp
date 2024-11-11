/*
 * Copyright (c) 2024 David Leeds <davidesleeds@gmail.com>
 *
 * jstore is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <cstdio>
#include <optional>

#include <fmt/base.h>
#include <sdbus-c++/sdbus-c++.h>
#include <visit_struct/visit_struct.hpp>

#include <jstore.hpp>

using json = nlohmann::json;

/*
 * ADL serializer for for std::optional<T>
 */
namespace nlohmann {

template <typename T>
void to_json(json &j, const std::optional<T> &v)
{
    j = v ? json(*v) : json();
}

template <typename T>
void from_json(const json &j, std::optional<T> &v)
{
    v = j.is_null() ? std::nullopt : std::optional<T>{j.get<T>()};
}

} /* namespace nlohmann */


/*
 * Demo Wi-Fi manager configuration.
 */
namespace wifi {

enum class security {
    WEP,
    WPA,
    WPA2,
    WPA3
};

struct profile {
    std::string name;
    std::vector<uint8_t> ssid;
    std::vector<uint8_t> psk;
    security mode = security::WPA3;

    bool operator==(const profile &) const = default;
};

struct config {
    std::string country = "US";
    std::optional<uint32_t> selected_profile;
    std::map<uint32_t, profile> profiles;

    bool operator==(const config &) const = default;
};

} /* namespace wifi */

/* visit_struct metadata defined outside of wifi namespace */
VISITABLE_STRUCT(wifi::profile, name, ssid, psk, mode);
VISITABLE_STRUCT(wifi::config, country, selected_profile, profiles);


/*
 * Demo Wi-Fi manager application.
 */
namespace wifi {

class manager {
public:
    static inline const sdbus::ServiceName DBUS_SERVICE { "com.example.WifiManager" };
    static inline const sdbus::ObjectPath DBUS_OBJECT   { "/com/example/WifiManager" };

    manager(std::filesystem::path config_file) :
        config_(std::move(config_file), config_error_handler),
        dbus_conn_(sdbus::createBusConnection(DBUS_SERVICE)),
        dbus_object_(sdbus::createObject(*dbus_conn_, DBUS_OBJECT))
    {
        fmt::println(stderr, "starting Wi-Fi manager");

        /* Register the io.davidleeds.JStore interface with the D-Bus object */
        config_.register_dbus(*dbus_object_);

        /* Save and apply updated config when set via D-Bus */
        config_.dbus().on_set([this](const std::string &path) {
            config_.save();
            apply();
        });
    }

    const auto &config() const
    {
        return config_.root();
    }

    void run()
    {
        /*
         * Consume calling thread to service the D-Bus connection. This is
         * for demonstration purposes only, and a real application would likely
         * integrate sdbus-c++ with its event loop to allow it to coexist with
         * other events on the same thread.
         */
        dbus_conn_->enterEventLoop();
    }

    void set_profile(uint32_t id, profile p)
    {
        config_->profiles[id] = std::move(p);
    }

    void select_profile(uint32_t id)
    {
        config_->selected_profile = id;
    }

    void apply()
    {
        fmt::println(stderr, "applying config: profile[{}] selected", config_->selected_profile.value_or(0));

        /* TODO apply current configuration */
    }

private:
    /*
     * Error callback for config serialization and deserialzation errors.
     */
    static void config_error_handler(std::string &&msg)
    {
        fmt::println(stderr, "config error: {}", msg);
    }

    jstore::tree<struct config> config_;
    std::unique_ptr<sdbus::IConnection> dbus_conn_{sdbus::createBusConnection(DBUS_SERVICE)};
    std::unique_ptr<sdbus::IObject> dbus_object_{sdbus::createObject(*dbus_conn_, DBUS_OBJECT)};
};

} /* namespace wifi */


int main(int argc, char **argv)
{
    /* Create hypothetical Wi-Fi manager application */
    wifi::manager wm{"/tmp/test/jstore/wifi.conf"};

    /* Add a default profile if none are present */
    if (wm.config().profiles.empty()) {
        wm.set_profile(42, {
            .name = "Work",
            .ssid = { 'I', 'n', 'i', 't', 'e', 'c', 'h' },
            .psk = { 'S', 't', 'a', 'p', 'l', 'e', 'r' }
        });
        wm.select_profile(42);
        wm.apply();
    }

    /* Enter the event loop to process incoming messages */
    wm.run();

    return 0;
}
