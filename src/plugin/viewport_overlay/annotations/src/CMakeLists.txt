project(annotations_tool VERSION 0.1.0 LANGUAGES CXX)

find_package(Imath)
find_package(Qt5 COMPONENTS Core Quick Gui REQUIRED)

set(SOURCES
	annotations_tool.cpp
	annotation.cpp
	annotation_opengl_renderer.cpp
	annotation_serialiser.cpp
	serialisers/1.0/serialiser_1_pt_0.cpp	
)

qt5_add_resources(SOURCES qml/AnnotationsTool.2/anno_tools.qrc)

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
		Qt5::Core
		Qt5::Quick
		Qt5::Gui
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)