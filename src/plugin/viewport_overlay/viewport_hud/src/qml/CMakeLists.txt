project(basic_viewport_ui VERSION 0.1.0 LANGUAGES CXX)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ViewportHUD/ DESTINATION share/xstudio/plugin/qml/ViewportHUD)

add_custom_target(COPY_VHUD_QML ALL)

add_custom_command(TARGET COPY_VHUD_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ViewportHUD ${CMAKE_BINARY_DIR}/bin/plugin/qml/ViewportHUD)
                         