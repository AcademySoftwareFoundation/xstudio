project(pixel_probe VERSION 0.1.0 LANGUAGES CXX)

if(WIN32)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/PixelProbe.1/ DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/PixelProbe.1)
else()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/PixelProbe.1/ DESTINATION share/xstudio/plugin/qml/PixelProbe.1)
endif()

add_custom_target(COPY_PIXELPROBE_QML ALL)

add_custom_command(TARGET COPY_PIXELPROBE_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/PixelProbe.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/PixelProbe.1)

