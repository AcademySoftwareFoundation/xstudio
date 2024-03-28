project(grading VERSION 0.1.0 LANGUAGES CXX)

find_package(Imath)
find_package(OpenColorIO CONFIG)

set(SOURCES
	grading_data.cpp
	grading_data_serialiser.cpp
	grading_colour_op.cpp
	grading.cpp
	grading_mask_gl_renderer.cpp
	serialisers/1.0/serialiser_1_pt_0.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::colour_pipeline::grading ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::module
		xstudio::plugin_manager
		xstudio::colour_pipeline
		xstudio::ui::opengl::viewport
		Imath::Imath
		OpenColorIO::OpenColorIO
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

add_subdirectory(qml)