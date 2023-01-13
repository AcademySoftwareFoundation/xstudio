SET(LINK_DEPS
	xstudio::data_source
	xstudio::shotgun_client
	xstudio::playlist
	xstudio::utility
	xstudio::event
	xstudio::module
	xstudio::media
)

create_plugin_with_alias(data_source_shotgun xstudio::data_source::shotgun 0.1.0  "${LINK_DEPS}")

add_subdirectory(qml)
