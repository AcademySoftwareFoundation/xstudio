find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat swscale avutil)
configure_file(.clang-tidy .clang-tidy)

SET(LINK_DEPS
	xstudio::media_metadata
	FFMPEG::avcodec
	FFMPEG::avformat
	FFMPEG::swscale
	FFMPEG::avutil
)

create_plugin_with_alias(media_metadata_ffprobe xstudio::media_metadata::ffprobe 0.1.0  "${LINK_DEPS}")
