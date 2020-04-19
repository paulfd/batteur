project(batteur)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK "jack" REQUIRED)
add_executable (batteur_jack batteur_jack.cpp)
target_include_directories (batteur_jack PRIVATE ${JACK_INCLUDE_DIRS})
target_link_libraries (batteur_jack PRIVATE batteur::batteur ${JACK_LIBRARIES})
