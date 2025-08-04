
SET(LINK_DEPS
	xstudio::json_store
	xstudio::utility
	CAF::core
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	xstudio::json_store_static
	CAF::core
)

create_component_static(media ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${STATIC_LINK_DEPS}")