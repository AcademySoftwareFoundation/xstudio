
SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	xstudio::media
	CAF::core
)

create_component(bookmark ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")