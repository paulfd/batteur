set (LV2PLUGIN_PRJ_NAME "${PROJECT_NAME}_lv2")

# Set the build directory as <build_dir>/lv2/<plugin_name>.lv2/
set (PROJECT_BINARY_DIR "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.lv2")

# LV2 plugin specific settings
include (LV2Config)

# Keep non build turtle files in IDE
set (LV2PLUGIN_TTL_SRC_FILES
    manifest.ttl.in
    ${PROJECT_NAME}.ttl.in
)
add_library (${LV2PLUGIN_PRJ_NAME} MODULE ${PROJECT_NAME}.c ${LV2PLUGIN_TTL_SRC_FILES})
target_link_libraries (${LV2PLUGIN_PRJ_NAME} ${PROJECT_NAME}::${PROJECT_NAME})
target_include_directories(${LV2PLUGIN_PRJ_NAME} PRIVATE .)

# Explicitely strip all symbols on Linux but lv2_descriptor()
# MacOS linker does not support this apparently https://bugs.webkit.org/show_bug.cgi?id=144555
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(COPY lv2.version DESTINATION ${CMAKE_BINARY_DIR}/lv2)
    target_link_libraries(${LV2PLUGIN_PRJ_NAME} "-Wl,--version-script=lv2.version")
endif()
if (MINGW)
    set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES LINK_FLAGS "-static")
endif()

# Remove the "lib" prefix, rename the target name and build it in the .lv build dir
# <build_dir>/lv2/<plugin_name>_lv2.<ext> to
# <build_dir>/lv2/<plugin_name>.lv2/<plugin_name>.<ext>
set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES PREFIX "")
set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# Generate *.ttl files from *.in sources,
# create the destination directory if it doesn't exists and copy needed files
file (MAKE_DIRECTORY ${PROJECT_BINARY_DIR})
configure_file (manifest.ttl.in ${PROJECT_BINARY_DIR}/manifest.ttl)
configure_file (${PROJECT_NAME}.ttl.in ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.ttl)
configure_file (modgui.ttl.in ${PROJECT_BINARY_DIR}/modgui.ttl)
add_custom_command(TARGET ${LV2PLUGIN_PRJ_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/lv2/modgui"
        "${PROJECT_BINARY_DIR}/modgui"
)

add_custom_command(TARGET ${LV2PLUGIN_PRJ_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/beats"
        "${PROJECT_BINARY_DIR}/beats"
)

# Installation
if (NOT MSVC)
    install (DIRECTORY ${PROJECT_BINARY_DIR} DESTINATION ${LV2PLUGIN_INSTALL_DIR}
        COMPONENT "lv2")
endif()