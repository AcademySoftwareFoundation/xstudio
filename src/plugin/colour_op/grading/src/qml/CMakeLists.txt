project(grading VERSION 0.1.0 LANGUAGES CXX)

if(WIN32)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Grading.1/ DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/Grading.1)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/MaskTool.1/ DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/MaskTool.1)
else()
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Grading.1/ DESTINATION share/xstudio/plugin/qml/Grading.1)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/MaskTool.1/ DESTINATION share/xstudio/plugin/qml/MaskTool.1)
endif()



add_custom_target(COPY_GRADE_QML ALL)

add_custom_command(TARGET COPY_GRADE_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Grading.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/Grading.1)

add_custom_command(TARGET COPY_GRADE_QML POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                        copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/MaskTool.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/MaskTool.1)
