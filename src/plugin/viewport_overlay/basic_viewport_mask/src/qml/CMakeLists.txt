project(basic_viewport_ui VERSION 0.1.0 LANGUAGES CXX)

if(WIN32)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/BasicViewportMask.1/ DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/BasicViewportMask.1)
else()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/BasicViewportMask.1/ DESTINATION share/xstudio/plugin/qml/BasicViewportMask.1)
endif()

add_custom_target(COPY_BVP_QML ALL)

add_custom_command(TARGET COPY_BVP_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/BasicViewportMask.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/BasicViewportMask.1)

