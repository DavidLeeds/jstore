# jstore

[![CI](https://github.com/DavidLeeds/jstore/actions/workflows/ci.yml/badge.svg)](https://github.com/DavidLeeds/jstore/actions/workflows/ci.yml)

`jstore` is a header-only library that allows application configuration and state in C++ classes and structures to be persisted to flash and exported via RPC (D-Bus). In addition to the `jstore::tree<T>` class that wraps application-defined state, the library exposes a tool-kit of library functions for serialization and path-based traversal of the tree. `jstore` is written in modern C++ and requires the '20 language standard.

## Design Philosophy

* **Minimize impact to the application.** While the `jstore::tree<T>` class wraps application state, state is represented using native C++ data types and may be directly accessed by the application. This contrasts with traditional databases or configuration stores where glue code must be written to load and store every piece of state. In most cases, applications pass references to their state structures with no knowledge that persistence and export is being managed by `jstore`.
* **Infinite potential for extension.** For simplicity and broad compatibility with existing code-bases, `jstore` uses the wildly popular [nlohmann/json](https://github.com/nlohmann/json) library for serialization and deserialization. This library has built-in support for most common STL data types, and allows applications to define their own `from_json()` and `to_json()` ADL serializers for any data type. While JSON is not the most compact serialization format, it maps well to most data models, is human-readable, and is approachable by a broad audience. Compression of on-disk data is a likely future feature.
* **Leverage data type reflection.** While proposals have been circulated for built-in static reflection in the C++ '26 language standard, we can be assured that it will a number of years until it is available to the average application. Until then, we can take our pick from several open-source libraries that utilize preprocessor macros to provide compile-time reflection of classes and structs. While the macro solution is admittedly ugly, the benefits appear to outweigh the aesthetic shortcomings. I selected the feature-rich and stable [cbeck88/visit_struct](https://github.com/cbeck88/visit_struct) library for this purpose. `visit_struct` allows us to iterate over members of arbitrary classes, which is a core piece of `jstore`'s strategy for serialization and path-based-traversal.

## Library Usage

Here are some step-by-step examples of how to use the library.

### Integrating your application with `jstore`
 
 1. Define your application state. Generally, a C++ structure with default values and `visit_struct` metadata is ideal. You may want to create a hierarchy of sub-objects and use STL container types. Below, we define configuration for a hypothetical Wi-Fi manager:

```c++
struct wifi_profile {
    std::string name;
    std::vector<uint8_t> ssid;
    std::vector<uint8_t> psk;
    security security = security::WPA3;
};
VISITABLE_STRUCT(wifi_profile, name, ssid, psk, security);

struct wifi_config {
    std::string country = "US";
    std::optional<uint32_t> selected_profile;
    std::map<uint32_t, wifi_profile> profiles;
};
VISITABLE_STRUCT(wifi_config, country, selected_profile, profiles);
```

> Note: [nlohmann/json](https://github.com/nlohmann/json) does not provide a built-in serializer for `std::optional`. Thus, a user-defined ADL serializer must be defined for the above example to work. See [How do I convert third-party types?](https://github.com/nlohmann/json?tab=readme-ov-file#how-do-i-convert-third-party-types)

2. Construct a `jstore::tree<T>` object that wraps the root configuration type. If a valid file exists, it will be loaded and `wifi_config` will be initialized from its contents.

```c++
jstore::tree<wifi_config> config{"/etc/config/wifi.conf"};
```

3. Access the underlying config object by calling `jstore::tree<T>::root()`, or using the dereference (`->`) operator:

```c++
if (config->selected_profile.has_value()) {
    std::println("Wi-Fi connection: {}", config->profiles.at(config->selected_profile.value()).name);
} else {
    std::println("No Wi-Fi connection selected");
}
```

### Updating and persisting your application state

1. Make some changes to the in-memory state:
```c++
config->profiles[42] = {
    .name = "Work",
    .ssid = { 'I', 'n', 'i', 't', 'e', 'c', 'h' },
    .psk = { 'S', 't', 'a', 'p', 'l', 'e', 'r' }
};

config->selected_profile = 42;
```

2. Update the config file:
```c++
config.save();
```

### Loading changes to the underlying file

To pull in on-disk changes or re-sync the in-memory state to the file contents:

```c++
config.load();
```

### Handling errors

By default, the `jstore` library ignores serialization and deserialization errors for individual tree members. This provides resilience to changing configuration formats or data corruption. Any members that cannot be parsed are reverted to their default values. Applications that wish to log these failures or perform additional error handling may specify an error handling callback.

```c++
auto on_error = [](std::string_view msg) {
    std::println(stderr, "config error: {}", msg);
};

jstore::tree<wifi_config> config{"/etc/config/wifi.conf", on_error};
```

### Iterating over tree members

It may be useful to visit each tree member. The `jstore::tree<T>::for_each()` function facilitates this interaction:

```c++
void dump_config()
{
    config.for_each([this](const string &path, const auto &member) {
        using member_type = decay_t<decltype(member)>;
        
        if constexpr (std::formattable<member_type>) {
            std::println("{} = {}", path, member);
        } else {
            std::println("{} = ???", path);
        }
    });
}
```

## Integration

Coming soon
