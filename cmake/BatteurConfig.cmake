set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard to be used")
set(CMAKE_C_STANDARD 99 CACHE STRING "C standard to be used")

# Export the compile_commands.json file
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only install what's explicitely said
set (CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

# Set Windows compatibility level to 7
if (WIN32)
    add_compile_definitions(_WIN32_WINNT=0x601)
endif()

# The variable CMAKE_SYSTEM_PROCESSOR is incorrect on Visual studio...
# see https://gitlab.kitware.com/cmake/cmake/issues/15170

if (NOT BATTEUR_SYSTEM_PROCESSOR)
    if(MSVC)
        set(BATTEUR_SYSTEM_PROCESSOR "${MSVC_CXX_ARCHITECTURE_ID}")
    else()
        set(BATTEUR_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

# Add required flags for the builds
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall)
    add_compile_options(-Wextra)
    add_compile_options(-ffast-math)
    add_compile_options(-fno-omit-frame-pointer) # For debugging purposes
    if (BATTEUR_SYSTEM_PROCESSOR MATCHES "^(i.86|x86_64)$")
        add_compile_options(-msse2)
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_STANDARD 17)
    add_compile_options(/Zc:__cplusplus)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# If we build with Clang use libc++
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT ANDROID)
    set(USE_LIBCPP ON CACHE BOOL "Use libc++ with clang")
    if (USE_LIBCPP)
    add_compile_options(-stdlib=libc++)
        # Presumably need the above for linking too, maybe other options missing as well
        add_link_options(-stdlib=libc++)   # New command on CMake master, not in 3.12 release
        add_link_options(-lc++abi)   # New command on CMake master, not in 3.12 release
    endif()
endif()

include (CheckLibraryExists)
add_library (batteur-atomic INTERFACE)
if (UNIX AND NOT APPLE)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic/check_libatomic.c" "int main() { return 0; }")
    try_compile(BATTEUR_LINK_LIBATOMIC "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic"
        SOURCES "${CMAKE_CURRENT_BINARY_DIR}/check_libatomic/check_libatomic.c"
        LINK_LIBRARIES "atomic")
    if (BATTEUR_LINK_LIBATOMIC)
        target_link_libraries (batteur-atomic INTERFACE "atomic")
    endif()
else()
    set(BATTEUR_LINK_LIBATOMIC FALSE)
endif()

# Don't show build information when building a different project
function (show_build_info_if_needed)
    if (CMAKE_PROJECT_NAME STREQUAL "batteur")
        message (STATUS "
Project name:                  ${PROJECT_NAME}
Build type:                    ${CMAKE_BUILD_TYPE}
Build processor:               ${BATTEUR_SYSTEM_PROCESSOR}
Build JACK stand-alone client: ${BATTEUR_JACK}
Build LV2 plug-in:             ${BATTEUR_LV2}
Build tests:                   ${BATTEUR_TESTS}
Use libc++:                    ${BATTEUR_TESTS}
Link libatomic:                ${USE_LIBCPP}

Install prefix:                ${CMAKE_INSTALL_PREFIX}
LV2 destination directory:     ${LV2PLUGIN_INSTALL_DIR}

Compiler CXX debug flags:      ${CMAKE_CXX_FLAGS_DEBUG}
Compiler CXX release flags:    ${CMAKE_CXX_FLAGS_RELEASE}
Compiler CXX min size flags:   ${CMAKE_CXX_FLAGS_MINSIZEREL}
")
    endif()
endfunction()
