SET(LINK_DEPS
	xstudio::utility
	xstudio::plugin_manager
	xstudio::module
	CAF::core
)

create_component(colour_pipeline ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
