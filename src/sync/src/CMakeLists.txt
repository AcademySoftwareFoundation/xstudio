SET(LINK_DEPS
	xstudio::utility
	xstudio::global_store
	xstudio::media
	caf::core
	caf::io
)

create_component(sync 0.1.0 "${LINK_DEPS}")
