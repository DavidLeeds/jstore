# -------------------------------
# JSON Configuration Storage library
# -------------------------------

cmake_minimum_required(VERSION 3.16)
project(
    jstore
    VERSION 0.2.0
    LANGUAGES CXX
)

# -------------------------------
# Project Options
# -------------------------------

option(JSTORE_BUILD_JSON "Fetch the nlohmann_json library, instead of searching for it in the system" OFF)
option(JSTORE_BUILD_DBUS "Fetch the sdbus-c++ library, instead of searching for it in the system" OFF)
option(JSTORE_BUILD_TESTS "Build tests" OFF)
option(JSTORE_BUILD_EXAMPLES "Build examples" OFF)
option(JSTORE_ENABLE_DBUS "Enable remote access to the data model via D-Bus (sdbus-c++ library)" OFF)

# -------------------------------
# Setup Compiler
# -------------------------------

# Set compiler options
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------
# Resolve library dependencies
# -------------------------------

add_subdirectory(thirdparty)

# -------------------------------
# Build Library
# -------------------------------

add_library(jstore INTERFACE)
target_compile_features(jstore INTERFACE cxx_std_20)
target_include_directories(jstore INTERFACE include)
target_link_libraries(jstore INTERFACE nlohmann_json::nlohmann_json fmt::fmt visit_struct::visit_struct)

# Optional D-Bus integration using sdbus-c++ library
if(JSTORE_ENABLE_DBUS)
    target_compile_definitions(jstore INTERFACE JSTORE_SDBUSCPP=1)
    target_link_libraries(jstore INTERFACE SDBusCpp::sdbus-c++)
else()
    target_compile_definitions(jstore INTERFACE JSTORE_SDBUSCPP=0)
endif()

# -------------------------------
# Build Unit Tests
# -------------------------------

if(JSTORE_BUILD_TESTS)
    message(STATUS "jstore: building with tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# -------------------------------
# Build Examples
# -------------------------------

if(JSTORE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
