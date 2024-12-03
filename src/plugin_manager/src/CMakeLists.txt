SET(LINK_DEPS
	caf::core
	xstudio::module
	xstudio::utility
)

if(UNIX AND NOT APPLE)
    list(APPEND LINK_DEPS stdc++fs dl)
endif()

create_component(plugin_manager 0.1.0 "${LINK_DEPS}")
