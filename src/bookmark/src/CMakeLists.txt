
SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	xstudio::media
	caf::core
)

create_component(bookmark 0.1.0 "${LINK_DEPS}")