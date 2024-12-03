
SET(LINK_DEPS
	xstudio::json_store
	xstudio::utility
	caf::core
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	xstudio::json_store_static
	caf::core
)

create_component_static(media 0.1.0 "${LINK_DEPS}" "${STATIC_LINK_DEPS}")