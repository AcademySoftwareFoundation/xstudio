set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

FILE(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_library(quickfuture SHARED ${SOURCES})
target_compile_definitions(quickfuture PUBLIC QUICK_FUTURE_BUILD_PLUGIN)

target_link_libraries(quickfuture PUBLIC Qt5::Core Qt5::Quick)
target_include_directories(quickfuture PUBLIC src)

set(QML_FILES
    src/qmldir
    src/quickfuture.qmltypes
)

set_target_properties(quickfuture
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickFuture"
)

if(WIN32)
    install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/QuickFuture)
    install(TARGETS quickfuture LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/QuickFuture)
else()
    install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/xstudio/plugin/qml/QuickFuture)
    install(TARGETS quickfuture LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/share/xstudio/plugin/qml/QuickFuture)

    # This may not be the best solution. We need to install the quickfuture qml and
    # library at build time into the ./bin/plugin/qml/QuickFuture folder. This allows
    # us to run xstudio directly from the build target without doing an install
    add_custom_target(COPY_FUTURE_QML DEPENDS copy-cmds)
    set(QML_FUTURE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/qmldir
        ${CMAKE_CURRENT_SOURCE_DIR}/src/quickfuture.qmltypes
    )

    add_custom_command(OUTPUT copy-cmds POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E
                            make_directory ${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickFuture)

    foreach(QMLFile ${QML_FUTURE_FILES})
        add_custom_command(OUTPUT copy-cmds APPEND PRE_BUILD
                        COMMAND ${CMAKE_COMMAND} -E
                            copy ${QMLFile} ${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickFuture/)
    endforeach()
    add_dependencies(quickfuture COPY_FUTURE_QML)

endif()
