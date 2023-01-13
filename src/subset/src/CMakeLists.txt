SET(LINK_DEPS
	xstudio::playhead
	xstudio::utility
	xstudio::audio_output
	caf::core
)

create_component(subset 0.1.0 "${LINK_DEPS}")
