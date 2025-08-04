SET(LINK_DEPS
	xstudio::colour_pipeline
	xstudio::global_store
	xstudio::utility
	xstudio::module
	xstudio::media
	CAF::core
)

create_component(playhead ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
