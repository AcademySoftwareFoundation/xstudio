SET(LINK_DEPS
	xstudio::playlist
	xstudio::utility
	xstudio::bookmark
	caf::core
)

create_component(session 0.1.0 "${LINK_DEPS}")
