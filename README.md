# jstore

[![CI](https://github.com/DavidLeeds/jstore/actions/workflows/ci.yml/badge.svg)](https://github.com/DavidLeeds/jstore/actions/workflows/ci.yml)

`jstore` is a header-only library that allows application configuration and state in C++ classes and structures to be persisted to flash and exported via RPC. In addition to the `jstore::tree<T>` class that wraps application-defined state, the library exposes a tool-kit of library functions for serialization and path-based traversal of the tree. `jstore` is written in modern C++ and requires the '20 language standard.

## Design Philosophy

* **Minimize impact to the application.** While the `jstore::tree<T>` class wraps application state, state is represented using native C++ data types and may be directly accessed by the application. This contrasts with traditional databases or configuration stores where glue code must be written to load and store every piece of state. In most cases, applications pass references to their state structures with no knowledge that persistence and export is being managed by `jstore`.
* **Infinite potential for extension.** For simplicity and broad compatibility with existing code-bases, `jstore` uses the wildly popular [nlohmann/json](https://github.com/nlohmann/json) library for serialization and deserialization. This library has built-in support for most common STL data types, and allows applications to define their own `from_json()` and `to_json()` ADL serializers for any data type. While JSON is not the most compact serialization format, it maps well to most data models, is human-readable, and is approachable by a broad audience. Compression of on-disk data is a likely future feature.
* **Leverage data type reflection.** While proposals have been circulated for built-in static reflection in the C++ '26 language standard, we can be assured that it will a number of years until it is available to the average application. Until then, we can take our pick from several open-source libraries that utilize preprocessor macros to provide compile-time reflection of classes and structs. While the macro solution is admittedly ugly, the benefits appear to outweigh the aesthetic shortcomings. I selected the feature-rich and stable [cbeck88/visit_struct](https://github.com/cbeck88/visit_struct) library for this purpose. `visit_struct` allows us to iterate over members of arbitrary classes, which is a core piece of `jstore`'s strategy for serialization and path-based-traversal.

## Examples

Coming soon

## Integration

Coming soon
