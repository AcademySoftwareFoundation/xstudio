SET(LINK_DEPS
	xstudio::playlist
	xstudio::utility
	xstudio::bookmark
	CAF::core
)

create_component(session ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
