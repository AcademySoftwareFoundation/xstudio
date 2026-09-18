SET(LINK_DEPS
	CAF::core
	xstudio::audio_output
	xstudio::contact_sheet
	xstudio::media
	xstudio::playhead
	xstudio::subset
	xstudio::timeline
	xstudio::utility
)

create_component(playlist ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
