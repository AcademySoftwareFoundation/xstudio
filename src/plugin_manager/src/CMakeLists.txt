SET(LINK_DEPS
	CAF::core
	xstudio::module
	xstudio::utility
)

if(UNIX)
	list(APPEND LINK_DEPS dl)  # Link against stdc++fs for Linux
	if (NOT APPLE)
		list(APPEND LINK_DEPS stdc++fs)  # Link against stdc++fs for Linux
	endif()
endif()

create_component(plugin_manager ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
