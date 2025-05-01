SET(LINK_DEPS
	xstudio::media
	xstudio::media_cache
	xstudio::global_store
	xstudio::utility
	CAF::core
)

if(WIN32)
	#list(APPEND LINK_DEPS ghc_filesystem)  # Link against the MSVSLX implementation for Windows
elseif(UNIX)
	list(APPEND LINK_DEPS dl)  # Link against stdc++fs for Linux
	if (NOT APPLE)
		list(APPEND LINK_DEPS stdc++fs)  # Link against stdc++fs for Linux
	endif()
endif()

create_component(media_reader ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")