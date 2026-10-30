SET(LINK_DEPS
	xstudio::global_store
	CAF::core
)

if(WIN32)
	#list(APPEND LINK_DEPS ghc_filesystem)  # Link against the MSVSLX implementation for Windows
elseif(UNIX)
	list(APPEND LINK_DEPS dl)  # Link against ${XSTUDIO_STDCXXFS} for Linux
	if (NOT APPLE)
		list(APPEND LINK_DEPS ${XSTUDIO_STDCXXFS})  # Link against ${XSTUDIO_STDCXXFS} for Linux
	endif()
endif()

create_component(media_metadata ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
