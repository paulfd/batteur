include(cmake/CPM.cmake)

set(OPTIONAL_BUILD_PACKAGE OFF CACHE BOOL "")
set(OPTIONAL_BUILD_TESTS OFF CACHE BOOL "")
CPMFindPackage(
    NAME optional
    GITHUB_REPOSITORY "TartanLlama/optional"
    GIT_TAG v1.1.0
    DOWNLOAD_ONLY ${BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY}
)

set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "")
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "")
CPMFindPackage(
    NAME expected
    GITHUB_REPOSITORY "TartanLlama/expected"
    GIT_TAG v1.1.0
    DOWNLOAD_ONLY ${BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY}
)

CPMAddPackage(
    NAME atomic_queue
    GITHUB_REPOSITORY "max0x7ba/atomic_queue"
    GIT_TAG v1.5
    DOWNLOAD_ONLY ON
)
add_library(atomic_queue INTERFACE)
target_include_directories(atomic_queue INTERFACE ${atomic_queue_SOURCE_DIR}/include)

CPMFindPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.2
    DOWNLOAD_ONLY ${BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY}
)

CPMFindPackage(
    NAME ghc_filesystem
    GITHUB_REPOSITORY gulrak/filesystem
    VERSION 1.5.14
    DOWNLOAD_ONLY ${BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY}
)

CPMAddPackage(
    NAME lv2
    GITLAB_REPOSITORY lv2/lv2
    VERSION 1.18.10
    DOWNLOAD_ONLY ON
)
add_library(lv2 INTERFACE)
target_include_directories(lv2 INTERFACE ${lv2_SOURCE_DIR}/include)

if (BATTEUR_TESTS)
    CPMFindPackage(
        NAME Catch2
        GITHUB_REPOSITORY "catchorg/Catch2"
        GIT_TAG v3.5.0
        DOWNLOAD_ONLY ${BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY}
    )
endif()
