SET(LINK_DEPS
	xstudio::utility
	caf::io
	caf::core
)

create_component(caf_utility 0.1.0 "${LINK_DEPS}")

