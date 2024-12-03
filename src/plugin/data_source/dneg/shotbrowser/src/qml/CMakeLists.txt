install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ShotBrowser.1/ DESTINATION share/xstudio/plugin/qml/ShotBrowser.1)

add_custom_target(COPY_SHOTBROWSER_QML ALL)

add_custom_command(TARGET COPY_SHOTBROWSER_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ShotBrowser.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/ShotBrowser.1)