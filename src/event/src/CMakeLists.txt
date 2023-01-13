SET(LINK_DEPS
	xstudio::utility
	caf::core
)

create_component(event 0.1.0 "${LINK_DEPS}")