# After macdeployqt has done its thing to the bundle, it still needs
# some cleanup by removing absolute rpath's from and codesigning
# all the binaries.

find_package(Python COMPONENTS Interpreter REQUIRED)

set(_cmd "${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/macos_finalize_bundle.py ${CMAKE_BINARY_DIR}/xSTUDIO.app")

install(CODE "message(\"Running: ${_cmd}\")")
install(CODE "execute_process(COMMAND ${_cmd})")
