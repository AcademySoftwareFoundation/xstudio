SET(LINK_DEPS
	caf::core
	xstudio::audio_output
	xstudio::contact_sheet
	xstudio::media
	xstudio::playhead
	xstudio::subset
	xstudio::timeline
	xstudio::utility
)

create_component(playlist 0.1.0 "${LINK_DEPS}")
