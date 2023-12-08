SET(LINK_DEPS
	xstudio::utility
	xstudio::plugin_manager
	xstudio::module
	caf::core
)

create_component(colour_pipeline 0.1.0 "${LINK_DEPS}")
