set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Quick REQUIRED)

FILE(GLOB_RECURSE SOURCES src/*.cpp src/*.h)

add_library(quickfuture SHARED ${SOURCES})
target_compile_definitions(quickfuture PUBLIC QUICK_FUTURE_BUILD_PLUGIN)

target_link_libraries(quickfuture PUBLIC Qt6::Core Qt6::Quick)
target_include_directories(quickfuture PUBLIC src)

set(QML_FILES
    src/qmldir
    src/quickfuture.qmltypes
)

if(APPLE)
    set_target_properties(quickfuture
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/PlugIns/xstudio/qml/QuickFuture"
    )
else()
    set_target_properties(quickfuture
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickFuture")
endif()

install(FILES ${QML_FILES} DESTINATION share/xstudio/plugin/qml/QuickFuture)
install(TARGETS quickfuture LIBRARY DESTINATION share/xstudio/plugin/qml/QuickFuture)

if(WIN32)
    # On Windows the dll must be present in the qml/QuickFuture folder
    _install(TARGETS quickfuture RUNTIME DESTINATION share/xstudio/plugin/qml/QuickFuture)
endif()

# This may not be the best solution. We need to install the quickfuture qml and
# library at build time into the ./bin/plugin/qml/QuickFuture folder. This allows
# us to run xstudio directly from the build target without doing an install
add_custom_target(COPY_FUTURE_QML DEPENDS copy-cmds)
set(QML_FUTURE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qmldir
    ${CMAKE_CURRENT_SOURCE_DIR}/src/quickfuture.qmltypes
)

if (APPLE)
    set(QML_DEST_DIR ${CMAKE_BINARY_DIR}/xSTUDIO.app/Contents/PlugIns/xstudio/qml/QuickFuture)
else()
    set(QML_DEST_DIR ${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickFuture)
endif()

add_custom_command(OUTPUT copy-cmds POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                        make_directory ${QML_DEST_DIR})

foreach(QMLFile ${QML_FUTURE_FILES})
    add_custom_command(OUTPUT copy-cmds APPEND PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E
                        copy ${QMLFile} ${QML_DEST_DIR}/)
endforeach()
    
add_dependencies(quickfuture COPY_FUTURE_QML)
