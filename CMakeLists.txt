cmake_minimum_required(VERSION 3.14)
project (batteur VERSION 0.2.0 LANGUAGES CXX C)
set (PROJECT_DESCRIPTION "A library to play backing drum parts live.")

# External configuration CMake scripts
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (BatteurConfig)
option (BATTEUR_LV2                         "Enable LV2 plug-in build [default: OFF]" OFF)
option (BATTEUR_TESTS                       "Enable tests build [default: OFF]" OFF)
option (BATTEUR_TOOLS                       "Enable tools build [default: OFF]" OFF)
option (BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY  "Download all dependencies in the directory specified by CPM_SOURCE_CACHE" OFF)

include(cmake/dependencies.cmake)

if (NOT BATTEUR_DOWNLOAD_DEPENDENCIES_ONLY)
    add_subdirectory (src)

    if (BATTEUR_TESTS)
    add_subdirectory (tests)
    endif()

    if (BATTEUR_LV2)
    add_subdirectory (lv2)
    endif()

    if (BATTEUR_TOOLS)
    add_subdirectory (tools)
    endif()
endif()

show_build_info_if_needed()
