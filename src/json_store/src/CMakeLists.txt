SET(LINK_DEPS
	CAF::core
	xstudio::utility
)

SET(STATIC_LINK_DEPS
	xstudio::utility_static
	CAF::core
)

create_component_static(json_store ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${STATIC_LINK_DEPS}")
