SET(LINK_DEPS
	CAF::core
    OpenGL::GL
	OpenGL::GLU
	GLEW::GLEW
    xstudio::ui::base
    xstudio::ui::canvas
    xstudio::ui::viewport
    xstudio::utility
    xstudio::media_reader
    xstudio::colour_pipeline
	OpenEXR::OpenEXR
	Imath::Imath
)

if(UNIX)
    list(APPEND LINK_DEPS pthread)
endif()

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenEXR)
find_package(Imath)

create_component_with_alias(opengl_viewport xstudio::ui::opengl::viewport ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")

if (USE_VCPKG)
	find_package(freetype CONFIG REQUIRED)
else()
	find_package(Freetype REQUIRED)
	target_include_directories(opengl_viewport PUBLIC "${FREETYPE_INCLUDE_DIRS}")
endif()

if (APPLE)
target_compile_definitions(opengl_viewport PRIVATE GL_SILENCE_DEPRECATION=1)
endif()

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		freetype
	PUBLIC
		GLEW::GLEW
)
