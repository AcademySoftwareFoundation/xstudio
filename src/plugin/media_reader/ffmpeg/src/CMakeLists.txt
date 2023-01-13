project(media_reader_ffmpeg VERSION 0.1.0 LANGUAGES CXX)

find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat swscale avutil)


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

target_compile_options(${PROJECT_NAME} PRIVATE -Wfatal-errors)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		xstudio::media_reader
		FFMPEG::avcodec
		FFMPEG::avformat
		FFMPEG::swscale
		FFMPEG::avutil
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
