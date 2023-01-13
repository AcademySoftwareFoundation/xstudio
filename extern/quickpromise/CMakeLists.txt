#
# To build it with cmake, you should register qml types explicitly by calling registerQuickFluxQmlTypes() in your main.cpp
# See examples/middleware for example
#

cmake_minimum_required(VERSION 3.0.0)
project(quickpromise)

set(INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/
)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/qppromise.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/qptimer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/quickpromise.qrc
)

set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/qppromise.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/qptimer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/QuickPromise
)

