project(media_reader_ffmpeg VERSION 0.1.0 LANGUAGES CXX)

find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat swscale avutil swresample)
find_package(GLEW REQUIRED)

set(SOURCES
	ffmpeg_stream.cpp
	ffmpeg_decoder.cpp
	ffmpeg.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(xstudio::media_reader::ffmpeg ALIAS ${PROJECT_NAME})
default_plugin_options(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME}
	PUBLIC OPTIMISED_BUFFER=1
)

if(UNIX)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wfatal-errors)
endif()

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::media_reader
		GLEW::GLEW
		FFMPEG::avcodec
		FFMPEG::avformat
		FFMPEG::swscale
		FFMPEG::avutil
		FFMPEG::swresample
	)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)

if(WIN32)

# We want the externally defined ffmpeg dlls to be installed into the bin directory.
instalL(DIRECTORY ${FFMPEG_ROOT}/bin DESTINATION ${CMAKE_INSTALL_PREFIX}/ FILES_MATCHING PATTERN "*.dll")

# We don't want the vcpkg install, or it will install linked dlls.
_install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/plugin)
endif()
