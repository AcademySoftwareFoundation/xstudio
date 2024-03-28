SET(LINK_DEPS
	${CAF_LIBRARY_core}
    OpenGL::GL
	OpenGL::GLU
	GLEW::GLEW
    pthread
    xstudio::ui::base
    xstudio::ui::canvas
    xstudio::utility
    xstudio::media_reader
	OpenEXR::OpenEXR
	Imath::Imath
)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenEXR)
find_package(Imath)
find_package(Freetype)

create_component_with_alias(opengl_viewport xstudio::ui::opengl::viewport 0.1.0 "${LINK_DEPS}")

include_directories("${FREETYPE_INCLUDE_DIRS}")

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		freetype
	PUBLIC
		GLEW::GLEW
)
