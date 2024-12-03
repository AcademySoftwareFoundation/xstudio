SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	xstudio::global_store
	xstudio::timeline
	caf::core
)

create_component(conform 0.1.0 "${LINK_DEPS}")
