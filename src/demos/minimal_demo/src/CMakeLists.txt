project(minimal-player VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(SOURCES
	minimal_player.cpp
)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL REQUIRED)
find_package(OpenSSL)
find_package(ZLIB)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(${PROJECT_NAME} ${SOURCES})

default_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		xstudio::caf_utility
		xstudio::colour_pipeline
		xstudio::global
		xstudio::ui::viewport
		xstudio::ui::opengl::viewport
		xstudio::ui::qt::viewport_widget
		xstudio::utility
	PUBLIC
		caf::core
		$<$<BOOL:${NVIDIA_HACK}>:GLdispatch>
		Qt5::Gui
		Qt5::Widgets
		OpenSSL::SSL
		ZLIB::ZLIB
)


set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "${PROJECT_NAME}.bin"
	LINK_DEPENDS_NO_SHARED true
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin)

