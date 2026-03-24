SET(LINK_DEPS
	xstudio::media_reader
)

project(media_reader_pdf VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Pdf Gui Core Widgets Quick Concurrent REQUIRED)

set(SOURCES
	pdf.cpp
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::media_reader::pdf ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::utility
		xstudio::media_reader
	PRIVATE
		Qt6::Pdf
		Qt6::Core
		Qt6::Widgets
		Qt6::Quick
		Qt6::Concurrent
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
