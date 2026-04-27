# After everything else is installed, windeployqt will scan the contents and package up Qt dependencies.

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(windeployqt_exe windeployqt HINTS "${_qt_bin_dir}")

configure_file(qt_install.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/qt_install.cmake @ONLY)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/qt_install.cmake)
