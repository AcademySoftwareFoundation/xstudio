
SET(LINK_DEPS
	xstudio::json_store
	xstudio::playhead
	xstudio::utility
	caf::core
)

create_component(media 0.1.0 "${LINK_DEPS}")

