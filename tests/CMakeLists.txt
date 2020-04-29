project(batteur)

set(BATTEUR_TEST_SOURCES
    FilesT.cpp
    FileReadingT.cpp
    main.cpp
)
add_executable(batteur_tests ${BATTEUR_TEST_SOURCES})
target_link_libraries(batteur_tests PRIVATE batteur_objects)

file(COPY "files" DESTINATION ${CMAKE_BINARY_DIR}/tests)