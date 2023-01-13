SET(LINK_DEPS
	xstudio::media_reader
)

create_plugin_with_alias(media_reader_blank xstudio::media_reader::blank 0.1.0  "${LINK_DEPS}")
