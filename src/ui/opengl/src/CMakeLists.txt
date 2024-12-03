SET(LINK_DEPS
	${CAF_LIBRARY_core}
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
if(WIN32)
find_package(freetype CONFIG REQUIRED)
else()
find_package(Freetype)
include_directories("${FREETYPE_INCLUDE_DIRS}")
endif()

create_component_with_alias(opengl_viewport xstudio::ui::opengl::viewport 0.1.0 "${LINK_DEPS}")

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		freetype
	PUBLIC
		GLEW::GLEW
)
