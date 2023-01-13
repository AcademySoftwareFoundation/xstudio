
SET(LINK_DEPS
	xstudio::utility
	xstudio::broadcast
	xstudio::json_store
	caf::core
)

create_component(bookmark 0.1.0 "${LINK_DEPS}")