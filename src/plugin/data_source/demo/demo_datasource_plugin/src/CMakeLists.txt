###############################################################
#
# Find Dependencies
#
###############################################################

# set(xStudio_DIR "/user_data/ted/install/lib/cmake/xStudio")
# set(CAF_DIR "/builds/actor_framework/1.0.2/f3500984a1/lib64/cmake/CAF")

project(demo_plugin VERSION 0.1.0 LANGUAGES CXX)

#find_package(xStudio REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CAF COMPONENTS core REQUIRED)
find_package(OpenEXR REQUIRED)
find_package(Imath REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Qt6 COMPONENTS Core Concurrent Widgets Qml Quick REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

###############################################################
#
# xSTUDIO Demo Plugin
#
###############################################################

QT6_WRAP_CPP(SYNC_UI_MOC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/demo_plugin_database_ui_model.hpp")

set(SOURCES 
	demo_media_reader.cpp
	demo_plugin.cpp
	demo_plugin_database_ui_model.cpp
	DemoPlugin/resources/demo_resources.qrc
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

message("Building demo plugin for xSTUDIO version ${XSTUDIO_GLOBAL_VERSION}")

target_compile_definitions(${PROJECT_NAME}
		PRIVATE XSTUDIO_GLOBAL_VERSION=\"${XSTUDIO_GLOBAL_VERSION}\")

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

qt_add_qml_module(${PROJECT_NAME}
    URI DemoPlugin
	VERSION 1.0
	NO_GENERATE_PLUGIN_SOURCE 
	NO_PLUGIN_OPTIONAL 
	RESOURCE_PREFIX /
	PLUGIN_TARGET ${PROJECT_NAME}	
	QML_FILES
		DemoPlugin/CustomScrollBar.qml
		DemoPlugin/DemoPluginPanel.qml
		DemoPlugin/LoadableAssetsList.qml
		DemoPlugin/SequenceItem.qml
		DemoPlugin/SequenceShotTree.qml
		DemoPlugin/RenderItem.qml
		DemoPlugin/ShotItem.qml
		DemoPlugin/TreeBaseItem.qml
)

default_plugin_options(${PROJECT_NAME})

add_python_plugin(demo_plugin_python)

#install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib/xstudio/cpp-plugins)
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/plugin_sync_prefs.json DESTINATION lib/xstudio/cpp-plugins/preferences)