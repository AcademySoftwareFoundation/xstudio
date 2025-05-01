SET(LINK_DEPS
	xstudio::media_reader
)

create_plugin_with_alias(media_reader_blank xstudio::media_reader::blank ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")
