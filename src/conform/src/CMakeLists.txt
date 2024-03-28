SET(LINK_DEPS
	xstudio::utility
	xstudio::broadcast
	caf::core
)

create_component(conform 0.1.0 "${LINK_DEPS}")
