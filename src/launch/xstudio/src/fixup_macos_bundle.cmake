# Fix library paths in the app bundle to ensure they're relative
file(GLOB_RECURSE LIBRARIES "${APP_BUNDLE_DIR}/Contents/Frameworks/*.dylib")
foreach(LIB ${LIBRARIES})
    get_filename_component(LIB_NAME ${LIB} NAME)
    execute_process(COMMAND install_name_tool -id "@rpath/${LIB_NAME}" ${LIB})
endforeach()

# Fix executable references to libraries
execute_process(
    COMMAND
    install_name_tool
    -add_rpath
    "@executable_path/../Frameworks"
    ${APP_BUNDLE_DIR}/Contents/MacOS/xstudio.bin
)
