SET(LINK_DEPS
	xstudio::colour_pipeline
	xstudio::global_store
	xstudio::utility
	xstudio::module
	xstudio::media
	caf::core
)

create_component(playhead 0.1.0 "${LINK_DEPS}")
