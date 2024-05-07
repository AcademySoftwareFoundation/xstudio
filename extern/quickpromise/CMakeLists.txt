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
else()
    install(TARGETS quickpromise DESTINATION ${INSTALL_ROOT})
endif()

if(${STATIC})
# 
else()
    install(FILES ${QML_FILES} DESTINATION ${QML_INSTALL_DIR})
endif()

