SET(LINK_DEPS
	xstudio::utility
	xstudio::broadcast
	xstudio::json_store
	xstudio::global_store
	caf::core
)

create_component(conform 0.1.0 "${LINK_DEPS}")
