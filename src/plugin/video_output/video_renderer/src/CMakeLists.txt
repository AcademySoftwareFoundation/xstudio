project(video_renderer VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core REQUIRED)

set(SOURCES
	video_render_plugin.cpp
	video_render_worker.cpp
	video_render_gl_framegrab_to_yuv.cpp
	video_render_gl_framegrab_to_rgb10bit.cpp
)

qt6_add_resources(SOURCES qml/video_render_plugin.qrc)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::video_output::video_renderer ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::bookmark
		xstudio::module
		xstudio::plugin_manager
		xstudio::playlist
		xstudio::ui::qml::helper
		xstudio::ui::opengl::viewport
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_plugin_qml(${PROJECT_NAME} qml)
add_preference(video_render_prefs.json ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_NAME})
add_python_plugin(ffmpeg_encode)