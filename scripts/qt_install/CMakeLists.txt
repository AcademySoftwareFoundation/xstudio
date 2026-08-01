# After everything else is installed, windeployqt/macdeployqt will scan the contents and package up Qt dependencies.

get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

if(APPLE)
    find_program(macdeployqt_exe macdeployqt HINTS "${_qt_bin_dir}")
    set(_cmd "${macdeployqt_exe} ${CMAKE_BINARY_DIR}/xSTUDIO.app -qmldir=${CMAKE_SOURCE_DIR}/ui")
elseif(WIN32)
    find_program(windeployqt_exe windeployqt HINTS "${_qt_bin_dir}")
    set(_cmd "${windeployqt_exe} ${CMAKE_INSTALL_PREFIX}/bin/xstudio.exe --qmldir ${CMAKE_SOURCE_DIR}/ui)")
endif()

if(NOT "${_cmd}" STREQUAL "")
    install(CODE "message(\"Running: ${_cmd}\")")
    install(CODE "execute_process(COMMAND ${_cmd})")
endif()
