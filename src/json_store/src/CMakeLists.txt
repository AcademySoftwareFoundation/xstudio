SET(LINK_DEPS
	caf::core
	xstudio::utility
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	caf::core
)

create_component_static(json_store 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")
