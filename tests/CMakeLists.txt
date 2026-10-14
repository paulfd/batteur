project(batteur)

set(BATTEUR_TEST_SOURCES
    FilesT.cpp
    FileReadingT.cpp
    HelpersT.cpp
    PlayerT.cpp
    WrapperT.cpp
    ../src/wrapper.cpp
    main.cpp
)
add_executable(batteur_tests ${BATTEUR_TEST_SOURCES})
target_link_libraries(batteur_tests PRIVATE batteur_objects)

file(COPY "files" DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../beats" DESTINATION ${CMAKE_BINARY_DIR})

# The file-based tests expect fixtures under tests/files and beats relative to the working directory.
add_test(NAME batteur_tests COMMAND batteur_tests)
set_tests_properties(batteur_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
