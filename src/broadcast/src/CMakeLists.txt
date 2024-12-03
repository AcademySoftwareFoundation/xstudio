
SET(LINK_DEPS
	xstudio::utility
	caf::core
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	caf::core
)

create_component_static(broadcast 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")

