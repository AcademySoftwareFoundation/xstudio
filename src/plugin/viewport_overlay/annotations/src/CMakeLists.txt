project(annotations_tool VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

find_package(Imath)
find_package(Qt6 COMPONENTS Core Quick Gui REQUIRED)

set(ANNO_SYNC_EXTENSIONS On)

set(SOURCES
	annotations_tool.cpp
	annotation.cpp
	annotation_opengl_renderer.cpp
	annotation_serialiser.cpp
	serialisers/1.0/serialiser_1_pt_0.cpp	
)

if (ANNO_SYNC_EXTENSIONS)
	list(APPEND SOURCES sync_extensions.cpp)
endif()

qt6_add_resources(SOURCES qml/AnnotationsTool.2/anno_tools.qrc)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::viewport::annotations_tool ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

if (ANNO_SYNC_EXTENSIONS)
	target_compile_definitions(${PROJECT_NAME} PUBLIC ANNO_SYNC_EXTENSIONS)
endif()

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::ui::opengl::viewport
		Imath::Imath
		Qt6::Core
		Qt6::Quick
		Qt6::Gui
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_plugin_qml(${PROJECT_NAME} qml)