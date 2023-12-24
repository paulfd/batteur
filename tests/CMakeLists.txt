project(batteur)


list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
include(CTest)

set(BATTEUR_TEST_SOURCES
FilesT.cpp
FileReadingT.cpp
LibraryT.cpp
)
add_executable(batteur_tests ${BATTEUR_TEST_SOURCES})
target_link_libraries(batteur_tests PRIVATE batteur_static Catch2WithMain batteur)
catch_discover_tests(batteur_tests)

file(COPY "files" DESTINATION ${CMAKE_BINARY_DIR}/tests)
