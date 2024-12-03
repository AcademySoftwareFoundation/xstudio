SET(LINK_DEPS
	xstudio::utility
	xstudio::session
	caf::core
)

create_component(studio ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
