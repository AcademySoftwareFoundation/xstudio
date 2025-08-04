project(media_reader_ffmpeg VERSION ${XSTUDIO_GLOBAL_VERSION} LANGUAGES CXX)

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

if (${USE_VCPKG})

	find_package(FFMPEG REQUIRED)
	target_link_libraries(${PROJECT_NAME}
		PUBLIC
			xstudio::media_reader
			GLEW::GLEW
		PRIVATE 
			${FFMPEG_LIBRARIES}
		)
	target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
	target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})

else()

	find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat swscale avutil swresample)

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

endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED true)
