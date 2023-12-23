add_library(fmidi STATIC "fmidi/fmidi_mini.cpp")
target_include_directories(fmidi PUBLIC .)
target_compile_definitions(fmidi PUBLIC "FMIDI_STATIC=1" "FMIDI_DISABLE_DESCRIBE_API=1")
set_target_properties(fmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)

set (BATTEUR_SOURCES
    BeatDescription.cpp
    FileReadingHelpers.cpp
    Player.cpp
)

add_library(batteur_static ${BATTEUR_SOURCES})
target_link_libraries(batteur_static 
    PUBLIC
        tl::optional
        tl::expected
        atomic_queue
        nlohmann_json::nlohmann_json
        ghc_filesystem
    PRIVATE
        fmidi
)

target_compile_definitions(batteur_static PUBLIC NOMINMAX)
target_include_directories(batteur_static PUBLIC .)
set_target_properties(batteur_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(batteur::batteur ALIAS batteur_static)

add_library(batteur wrapper.cpp)
target_link_libraries(batteur PRIVATE batteur_static)
target_include_directories(batteur PUBLIC .)
target_compile_definitions(batteur PUBLIC BATTEUR_EXPORT_SYMBOLS)
set_target_properties (batteur PROPERTIES 
    SOVERSION ${PROJECT_VERSION_MAJOR} 
    OUTPUT_NAME batteur
    PUBLIC_HEADER "batteur.h"
)

if (NOT MSVC)
    install (TARGETS batteur
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT "batteur")
endif()
