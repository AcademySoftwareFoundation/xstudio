project(annotations_tool VERSION 0.1.0 LANGUAGES CXX)

find_package(Imath)

set(SOURCES
	annotations_tool.cpp
	annotation.cpp
	annotation_opengl_renderer.cpp
	annotation_serialiser.cpp
	caption.cpp
	pen_stroke.cpp
	serialisers/1.0/serialiser_1_pt_0.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::viewport::annotations_tool ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::ui::opengl::viewport
		Imath::Imath
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)