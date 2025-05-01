SET(LINK_DEPS
	xstudio::playhead
	xstudio::utility
	xstudio::audio_output
	CAF::core
)

create_component(subset ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
