SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	caf::core
)

create_component(tag 0.1.0 "${LINK_DEPS}")

