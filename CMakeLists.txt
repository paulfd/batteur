cmake_minimum_required(VERSION 3.5)
project (batteur VERSION 0.1.0 LANGUAGES CXX C)
set (PROJECT_DESCRIPTION "A library to play backing drum parts live.")

# External configuration CMake scripts
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (BatteurConfig)

option (BATTEUR_JACK              "Enable JACK stand-alone build [default: ON]" ON)
option (BATTEUR_LV2               "Enable LV2 plug-in build [default: ON]" OFF)
option (BATTEUR_TESTS             "Enable tests build [default: OFF]" OFF)
option (BATTEUR_SHARED            "Enable shared library build [default: ON]" ON)
option (BATTEUR_STATIC            "Enable static library build [default: ON]" ON)
option (BATTEUR_SYSTEM_FMT        "Use the system libfmt [default: ON]" ON)

if (NOT BATTEUR_SYSTEM_FMT)
    add_subdirectory(src/fmt EXCLUDE_FROM_ALL)
    # Add PIC to the fmt library
    set_target_properties(fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Alias it for fmidi
    add_library(fmidi-fmt ALIAS fmt)
endif()

add_subdirectory(src/fmidi EXCLUDE_FROM_ALL)

### Main lib
set (BATTEUR_SOURCES
    src/BeatDescription.cpp
    src/FileReadingHelpers.cpp
    src/Player.cpp
)

add_library(batteur_objects OBJECT ${BATTEUR_SOURCES})
target_link_libraries(batteur_objects PUBLIC fmt fmidi)
target_include_directories(batteur_objects PUBLIC src)
set_target_properties(batteur_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (BATTEUR_STATIC)
add_library(batteur_static STATIC src/wrapper.cpp)
target_link_libraries(batteur_static PRIVATE batteur_objects)
target_include_directories(batteur_static PUBLIC src)
add_library(batteur::batteur ALIAS batteur_static)
endif()

if (BATTEUR_SHARED)
add_library(batteur_shared SHARED src/wrapper.cpp)
target_link_libraries(batteur_shared PRIVATE batteur_objects)
target_include_directories(batteur_shared PUBLIC src)
endif()

if (BATTEUR_JACK)
add_subdirectory (clients)
endif()

if (BATTEUR_TESTS)
add_subdirectory (tests)
endif()

if (BATTEUR_LV2)
add_subdirectory (lv2)
endif()