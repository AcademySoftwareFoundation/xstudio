SET(LINK_DEPS
	xstudio::global_store
	caf::core
)

create_component(data_source 0.1.0 "${LINK_DEPS}")
