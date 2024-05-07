project(glx_minimal_player VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(SOURCES
	main.cpp
)

find_package(OpenGL REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif()
find_package(GLEW REQUIRED)
find_package(OpenSSL)
find_package(ZLIB)

add_executable(${PROJECT_NAME} ${SOURCES})

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		xstudio::caf_utility
		xstudio::colour_pipeline
		xstudio::global
		xstudio::ui::viewport
		xstudio::ui::opengl::viewport
		xstudio::utility
	PUBLIC
		caf::core
		$<$<BOOL:${NVIDIA_HACK}>:GLdispatch>
		OpenSSL::SSL
		ZLIB::ZLIB
		X11
)


set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "${PROJECT_NAME}.bin"
	LINK_DEPENDS_NO_SHARED true
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)

