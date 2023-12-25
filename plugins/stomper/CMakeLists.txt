get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set (LV2PLUGIN_VERSION_MINOR   1)
set (LV2PLUGIN_VERSION_MICRO   0)
set (LV2PLUGIN_NAME            ${PLUGIN_NAME})
set (LV2PLUGIN_COMMENT         "Backing drummer plugin")
set (LV2PLUGIN_URI             "https://github.com/paulfd/${PLUGIN_NAME}")
set (LV2PLUGIN_REPOSITORY      "https://github.com/paulfd/${PLUGIN_NAME}")
set (LV2PLUGIN_AUTHOR          "Paul Ferrand")
set (LV2PLUGIN_EMAIL           "paul@ferrand.cc")
set (LV2PLUGIN_SPDX_LICENSE_ID "ISC")

set(PLUGIN_TARGET_NAME "${PLUGIN_NAME}_lv2")

# Keep non build turtle files in IDE
set (LV2PLUGIN_TTL_SRC_FILES
    manifest.ttl.in
    ${PLUGIN_NAME}.ttl.in
    # modgui.ttl.in
)
add_library (${PLUGIN_TARGET_NAME} MODULE ${PLUGIN_NAME}.c ${LV2PLUGIN_TTL_SRC_FILES})
target_link_libraries (${PLUGIN_TARGET_NAME} batteur lv2)
target_include_directories(${PLUGIN_TARGET_NAME} PRIVATE .)

# Explicitely strip all symbols on Linux but lv2_descriptor()
# MacOS linker does not support this apparently https://bugs.webkit.org/show_bug.cgi?id=144555
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(COPY lv2.version DESTINATION ${CMAKE_BINARY_DIR}/lv2)
    target_link_libraries(${PLUGIN_TARGET_NAME} "-Wl,--version-script=lv2.version")
endif()
if (MINGW)
    set_target_properties (${PLUGIN_TARGET_NAME} PROPERTIES LINK_FLAGS "-static")
endif()
set_target_properties (${PLUGIN_TARGET_NAME} PROPERTIES PREFIX "")
set_target_properties (${PLUGIN_TARGET_NAME} PROPERTIES OUTPUT_NAME "${PLUGIN_NAME}")

configure_file (manifest.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl)
configure_file (${PLUGIN_NAME}.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.ttl)
# configure_file (modgui.ttl.in ${CMAKE_CURRENT_BINARY_DIR}/modgui.ttl)

install (
    FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/manifest.ttl
        ${CMAKE_CURRENT_BINARY_DIR}/${PLUGIN_NAME}.ttl
        # ${CMAKE_CURRENT_BINARY_DIR}/modgui.ttl 
    DESTINATION "${PLUGIN_NAME}.lv2" 
    COMPONENT "lv2"
)
install (
    TARGETS ${PLUGIN_TARGET_NAME} 
    LIBRARY DESTINATION "${PLUGIN_NAME}.lv2/Contents/Binary" 
    COMPONENT "lv2"
)
# install (
#     DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/modgui
#     DESTINATION "${PLUGIN_NAME}.lv2"
#     COMPONENT "lv2"
# )
install (
    DIRECTORY ${CMAKE_SOURCE_DIR}/beats
    DESTINATION "${PLUGIN_NAME}.lv2"
    COMPONENT "lv2"
)
