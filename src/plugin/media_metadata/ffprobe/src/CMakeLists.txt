configure_file(.clang-tidy .clang-tidy)

if (${USE_VCPKG})
    find_package(FFMPEG REQUIRED)
	SET(LINK_DEPS
		xstudio::media_metadata
		PRIVATE ${FFMPEG_LIBRARIES}
	)
	create_plugin_with_alias(media_metadata_ffprobe xstudio::media_metadata::ffprobe ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
	target_include_directories(media_metadata_ffprobe PRIVATE ${FFMPEG_INCLUDE_DIRS})
	target_link_directories(media_metadata_ffprobe PRIVATE ${FFMPEG_LIBRARY_DIRS})

else()
	find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat swscale avutil)
	SET(LINK_DEPS
		xstudio::media_metadata
		FFMPEG::avcodec
		FFMPEG::avformat
		FFMPEG::swscale
		FFMPEG::avutil
	)
	create_plugin_with_alias(media_metadata_ffprobe xstudio::media_metadata::ffprobe ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
endif()
