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
target_link_libraries (${LV2PLUGIN_PRJ_NAME} batteur lv2)
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
set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES PREFIX "")
set_target_properties (${LV2PLUGIN_PRJ_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

configure_file (manifest.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl)
configure_file (${PROJECT_NAME}.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.ttl)
configure_file (modgui.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/modgui.ttl)

install (
    FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.ttl
        ${CMAKE_CURRENT_BINARY_DIR}/modgui.ttl 
    DESTINATION "${PROJECT_NAME}.lv2" 
    COMPONENT "lv2"
)
install (
    TARGETS ${LV2PLUGIN_PRJ_NAME} 
    LIBRARY DESTINATION "${PROJECT_NAME}.lv2/Contents/Binary" 
    COMPONENT "lv2"
)
install (
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/modgui
    DESTINATION "${PROJECT_NAME}.lv2"
    COMPONENT "lv2"
)
install (
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../beats
    DESTINATION "${PROJECT_NAME}.lv2"
    COMPONENT "lv2"
)
