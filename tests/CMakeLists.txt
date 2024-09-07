include(FetchContent)

# Fetch Catch2 unit test framework at configure time
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)

# Fetch Sanitizer CMake helpers
FetchContent_Declare(
    SanitizersCMake
    GIT_REPOSITORY https://github.com/arsenm/sanitizers-cmake.git
    GIT_SHALLOW ON
)
FetchContent_GetProperties(SanitizersCMake)
if(NOT SanitizersCMake_POPULATED)
    FetchContent_Populate(SanitizersCMake)
    list(APPEND CMAKE_MODULE_PATH ${sanitizerscmake_SOURCE_DIR}/cmake)
endif()

# Enable ASan and UBSan
find_package(Sanitizers)
set(SANITIZE_ADDRESS TRUE)
set(SANITIZE_UNDEFINED TRUE)

include(Catch)

# Test core library
add_executable(ut_jstore ut_jstore.cpp ut_stdio_fstream.cpp)
add_sanitizers(ut_jstore)
target_link_libraries(ut_jstore jstore Catch2::Catch2WithMain)
catch_discover_tests(ut_jstore)

# Test optional D-Bus extensions
if(JSTORE_ENABLE_DBUS)
    target_sources(ut_jstore PRIVATE ut_jstore_dbus.cpp)
    target_link_libraries(ut_jstore SDBusCpp::sdbus-c++)
endif()
