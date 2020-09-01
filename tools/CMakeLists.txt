add_executable(batteur-serialize serialize.cpp)
target_link_libraries (batteur-serialize ${PROJECT_NAME}::${PROJECT_NAME})

if (NOT MSVC)
    install (TARGETS batteur-serialize
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT "runtime")
endif()