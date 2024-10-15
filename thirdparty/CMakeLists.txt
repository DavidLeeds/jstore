include(FetchContent)

# -------------------------------
# nlohmann_json
# -------------------------------

set(JSTORE_JSON_VERSION "3.11.3" CACHE STRING "nlohmann_json version to use in the project")

if(NOT JSTORE_BUILD_JSON)
    message(STATUS "jstore: using system nlohmann_json")
    find_package(nlohmann_json ${JSTORE_JSON_VERSION} REQUIRED)
else()
    message(STATUS "jstore: downloading nlohmann_json v${JSTORE_JSON_VERSION}")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v${JSTORE_JSON_VERSION}
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# -------------------------------
# fmt
# -------------------------------

set(JSTORE_FMT_VERSION "11.0.2" CACHE STRING "fmt version to use in the project")

# TODO check std::format support
include(CheckIncludeFileCXX)
check_include_file_cxx("format" JSTORE_STD_FORMAT_SUPPORT)

message(STATUS "jstore: downloading fmt v${JSTORE_FMT_VERSION}")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG ${JSTORE_FMT_VERSION}
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(fmt)

# -------------------------------
# visit_struct
# -------------------------------

set(JSTORE_VISIT_STRUCT_VERSION "1.1.0" CACHE STRING "visit_struct version to use in the project")

message(STATUS "jstore: downloading visit_struct v${JSTORE_VISIT_STRUCT_VERSION}")
FetchContent_Declare(
    visit_struct
    GIT_REPOSITORY https://github.com/cbeck88/visit_struct.git
    GIT_TAG v${JSTORE_VISIT_STRUCT_VERSION}
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(visit_struct)

# visit_struct does not have CMake integration, so manually import it
FetchContent_GetProperties(visit_struct)
add_library(visit_struct::visit_struct INTERFACE IMPORTED GLOBAL)
target_include_directories(visit_struct::visit_struct INTERFACE ${visit_struct_SOURCE_DIR}/include)
target_compile_features(visit_struct::visit_struct INTERFACE "cxx_std_11")

# -------------------------------
# sdbus-c++
# -------------------------------

if(JSTORE_ENABLE_DBUS)
    set(JSTORE_DBUS_VERSION "2.0.0" CACHE STRING "sdbus-c++ version to use in the project")

    if(NOT JSTORE_BUILD_DBUS)
        message(STATUS "jstore: using system sdbus-c++")
        find_package(sdbus-c++ ${JSTORE_DBUS_VERSION} REQUIRED)
    else()
        message(STATUS "jstore: downloading sdbus-c++ v${JSTORE_DBUS_VERSION}")
        FetchContent_Declare(
            sdbus-c++
            GIT_REPOSITORY https://github.com/Kistler-Group/sdbus-cpp.git
            GIT_TAG v${JSTORE_DBUS_VERSION}
            GIT_SHALLOW ON
        )
        set(BUILD_SHARED_LIBS OFF)
        set(SDBUSCPP_BUILD_LIBSYSTEMD ON)
        FetchContent_MakeAvailable(sdbus-c++)
        add_library(SDBusCpp::sdbus-c++ ALIAS sdbus-c++)
    endif()
endif()
