###############################################################
#
# Find Dependencies
#
###############################################################

project(filesystem_browser VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Qt6 COMPONENTS Core Concurrent Widgets Qml Quick REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

###############################################################
#
# xSTUDIO File Browser Plugin
#
###############################################################

QT6_WRAP_CPP(SYNC_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/scan_results_datamodel_ui.hpp")

set(SOURCES 
	scan_results_datamodel_ui.cpp
	FileSystemBrowser/resources/file_system_browser.qrc
	${SYNC_UI_MOC_SRC}) 

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::ui::qml::helper
		xstudio::utility
		xstudio::plugin_manager
		xstudio::thumbnail
		xstudio::bookmark
		CAF::core
		Qt6::Core
		Qt6::Qml
		Qt6::Quick
		Qt6::Widgets
		Qt6::Concurrent
)

target_compile_definitions(${PROJECT_NAME}
		PRIVATE XSTUDIO_GLOBAL_VERSION=\"${XSTUDIO_GLOBAL_VERSION}\")

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

file(GLOB QML_SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} FileSystemBrowser/xstudio/*.qml)

qt_add_qml_module(${PROJECT_NAME}
    URI FileSystemBrowser
	VERSION 1.0
	NO_GENERATE_PLUGIN_SOURCE 
	NO_PLUGIN_OPTIONAL 
	RESOURCE_PREFIX /
	PLUGIN_TARGET ${PROJECT_NAME}	
	QML_FILES ${QML_SOURCES}
)

default_plugin_options(${PROJECT_NAME})

add_python_plugin(filesystem_browser_python)
