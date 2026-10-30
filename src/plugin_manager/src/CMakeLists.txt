SET(LINK_DEPS
	CAF::core
	xstudio::module
	xstudio::utility
	xstudio::media_reader
)

if(UNIX)
	list(APPEND LINK_DEPS dl)  # Link against ${XSTUDIO_STDCXXFS} for Linux
	if (NOT APPLE)
		list(APPEND LINK_DEPS ${XSTUDIO_STDCXXFS})  # Link against ${XSTUDIO_STDCXXFS} for Linux
	endif()
endif()

create_component(plugin_manager ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
