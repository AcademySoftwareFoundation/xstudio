project(grading_demo VERSION 0.1.0 LANGUAGES CXX)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/GradingDemo.1/ DESTINATION share/xstudio/plugin/qml/GradingDemo.1)

add_custom_target(COPY_GRADE_DEMO_QML ALL)

add_custom_command(TARGET COPY_GRADE_DEMO_QML POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E
                         copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/GradingDemo.1 ${CMAKE_BINARY_DIR}/bin/plugin/qml/GradingDemo.1)

