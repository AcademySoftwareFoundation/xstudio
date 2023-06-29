project(grading_demo VERSION 0.1.0 LANGUAGES CXX)

find_package(Imath)

set(SOURCES
	grading_demo.cpp	
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::colour_pipeline::grading_demo ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::colour_pipeline
		xstudio::ui::opengl::viewport
		Imath::Imath
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)