project(viewport_ui VERSION 0.1.0 LANGUAGES CXX)

find_package(OpenEXR)
find_package(Imath)

set(SOURCES
	hud_plugin.cpp
	viewport.cpp
	viewport_frame_queue_actor.cpp
	fps_monitor.cpp
	keypress_monitor.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::ui::viewport ALIAS ${PROJECT_NAME})

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::utility
		xstudio::playhead
		OpenEXR::OpenEXR
		Imath::Imath
		caf::core
)

add_library(${PROJECT_NAME}_static STATIC ${SOURCES})
add_library(xstudio::ui::viewport_static ALIAS ${PROJECT_NAME}_static)

default_options_static(${PROJECT_NAME}_static)

target_link_libraries(${PROJECT_NAME}_static
	PUBLIC
		xstudio::utility_static
		OpenEXR::OpenEXR
		Imath::Imath
		caf::core
)
