SET(LINK_DEPS
	caf::core
	dl
	stdc++fs
	xstudio::broadcast
	xstudio::module
	xstudio::utility
)

create_component(plugin_manager 0.1.0 "${LINK_DEPS}")

