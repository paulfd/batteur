cmake_minimum_required(VERSION 3.5)
project (batteur VERSION 0.1.0 LANGUAGES CXX C)
set (PROJECT_DESCRIPTION "A library to play backing drum parts live.")

# External configuration CMake scripts
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (BatteurConfig)

option (BATTEUR_LV2               "Enable LV2 plug-in build [default: OFF]" OFF)
option (BATTEUR_TESTS             "Enable tests build [default: OFF]" OFF)
option (BATTEUR_TOOLS             "Enable tools build [default: OFF]" OFF)
option (BATTEUR_SHARED            "Enable shared library build [default: ON]" ON)
option (BATTEUR_STATIC            "Enable static library build [default: ON]" ON)

add_library(fmidi STATIC "src/fmidi/fmidi_mini.cpp")
target_include_directories(fmidi PUBLIC "src")
target_compile_definitions(fmidi PUBLIC "FMIDI_STATIC=1" "FMIDI_DISABLE_DESCRIBE_API=1")
set_target_properties(fmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)

### Main lib
set (BATTEUR_SOURCES
    src/BeatDescription.cpp
    src/FileReadingHelpers.cpp
    src/Player.cpp
)

add_library(batteur_objects OBJECT ${BATTEUR_SOURCES})
if (MSVC)
    target_compile_definitions(batteur_objects PUBLIC NOMINMAX)
endif()
target_link_libraries(batteur_objects PUBLIC fmidi)
target_include_directories(batteur_objects PUBLIC src)
set_target_properties(batteur_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (BATTEUR_STATIC)
    add_library(batteur_static STATIC src/wrapper.cpp)
    target_link_libraries(batteur_static PRIVATE batteur_objects)
    target_include_directories(batteur_static PUBLIC src)
    set_target_properties(batteur_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(batteur::batteur ALIAS batteur_static)
endif()

if (BATTEUR_SHARED)
    add_library(batteur_shared SHARED src/wrapper.cpp)
    target_link_libraries(batteur_shared PRIVATE batteur_objects)
    target_include_directories(batteur_shared PUBLIC src)
    target_compile_definitions(batteur_shared PRIVATE BATTEUR_EXPORT_SYMBOLS)
    set_target_properties (batteur_shared PROPERTIES 
        SOVERSION ${PROJECT_VERSION_MAJOR} 
        OUTPUT_NAME batteur
        PUBLIC_HEADER "src/batteur.h"
    )
    if (NOT MSVC)
        install (TARGETS batteur_shared
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT "runtime")
    endif()
endif()

if (BATTEUR_TESTS)
add_subdirectory (tests)
endif()

if (BATTEUR_LV2)
add_subdirectory (lv2)
endif()

if (BATTEUR_TOOLS)
add_subdirectory (tools)
endif()