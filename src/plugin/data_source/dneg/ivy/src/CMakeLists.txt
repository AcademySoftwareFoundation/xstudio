SET(LINK_DEPS
	xstudio::data_source
	xstudio::utility
	xstudio::module
	xstudio::media
	xstudio::http_client
)

create_plugin_with_alias(data_source_ivy xstudio::data_source::ivy ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")