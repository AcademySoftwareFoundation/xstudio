SET(LINK_DEPS
	xstudio::colour_pipeline
	xstudio::global_store
	xstudio::utility
	xstudio::module
	caf::core
)

create_component(playhead 0.1.0 "${LINK_DEPS}")
