SET(LINK_DEPS
	xstudio::media_reader
)

create_plugin_with_alias(media_reader_ppm xstudio::media_reader::ppm ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
