cmake_minimum_required(VERSION 3.14)

project(quickpromise LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INSTALL_ROOT ${CMAKE_INSTALL_PREFIX})

find_package(Qt5 COMPONENTS Core Quick REQUIRED)

if(NOT DEFINED STATIC)
  set(STATIC OFF)
endif()

set(QML_FILES
    qml/QuickPromise/promise.js
    qml/QuickPromise/qmldir
    qml/QuickPromise/Promise.qml
    qml/QuickPromise/PromiseTimer.qml
)

# Equivalent to QML_IMPORT_PATH
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${PROJECT_SOURCE_DIR}/qml)

# Approximates RESOURCES
qt5_add_resources(RESOURCES ${PROJECT_SOURCE_DIR}/qml/quickpromise.qrc)

# Add the library
if(${STATIC})
    add_library(quickpromise STATIC)
else()
    add_library(quickpromise SHARED)
endif()

target_link_libraries(quickpromise PRIVATE Qt5::Core Qt5::Quick)

# Add resource and header files to the library
target_sources(quickpromise PRIVATE ${QML_FILES} ${RESOURCES})

# Set install paths
set(QML_INSTALL_DIR ${INSTALL_ROOT}/bin/QuickPromise)

# Install the library and qml files
if(WIN32)
    install(TARGETS quickpromise DESTINATION ${INSTALL_ROOT}/bin)
    install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin/qml/QuickPromise)
else()
    install(TARGETS quickpromise DESTINATION ${INSTALL_ROOT})
    install(FILES ${QML_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/xstudio/plugin/qml/QuickPromise)
endif()

# QuickPromise needs to be installed somwhere on the QML include search
# paths. xstudio will add <xstudio bin location>/plugin/qml to the 
# search paths. Although quickpromise is installed as required when
# do a 'make install' moving it to the build destination allows us to
# run xstudio without an install for development environment.
add_custom_target(COPY_PROMISE_QML)
add_custom_command(TARGET COPY_PROMISE_QML POST_BUILD
                  COMMAND ${CMAKE_COMMAND} -E
                      copy_directory ${PROJECT_SOURCE_DIR}/qml/QuickPromise ${CMAKE_BINARY_DIR}/bin/plugin/qml/QuickPromise)
add_dependencies(quickpromise COPY_PROMISE_QML)