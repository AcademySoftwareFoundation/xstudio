project(wipe_layout_ui VERSION 0.1.0 LANGUAGES CXX)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/WipeLayoutOverlay.1/ DESTINATION share/xstudio/plugin/qml/WipeLayoutOverlay.1)

add_custom_target(COPY_WIPE_QML ALL)

add_custom_command(TARGET COPY_WIPE_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/WipeLayoutOverlay.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/WipeLayoutOverlay.1)

