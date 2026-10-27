find_package(OpenTime REQUIRED)
find_package(OpenTimelineIO REQUIRED)

SET(LINK_DEPS
	xstudio::data_source
	xstudio::utility
	xstudio::module
	xstudio::media
	xstudio::http_client
	OTIO::opentime
	OTIO::opentimelineio
)

create_plugin_with_alias(data_source_ivy xstudio::data_source::ivy ${XSTUDIO_GLOBAL_VERSION}  "${LINK_DEPS}")