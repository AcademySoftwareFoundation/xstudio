project(annotations_tool VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Imath)

find_package(Qt6 COMPONENTS Core Quick Gui REQUIRED)

set(SOURCES
	annotations_core_plugin.cpp
	annotations_ui_plugin.cpp
	annotation.cpp
	annotation_opengl_renderer.cpp
	annotation_serialiser.cpp
	annotation_undo_redo.cpp
	serialisers/1.0/serialiser_1_pt_0.cpp
	serialisers/2.0/serialiser_2_pt_0.cpp
)

qt6_add_resources(SOURCES qml/AnnotationsUI.2/anno_tools.qrc)

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
		Qt6::Core
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_plugin_qml(${PROJECT_NAME} qml)
