SET(LINK_DEPS
	xstudio::media
	xstudio::media_cache
	xstudio::global_store
	xstudio::broadcast
	caf::core
)

if(WIN32)
	#list(APPEND LINK_DEPS ghc_filesystem)  # Link against the MSVSLX implementation for Windows
elseif(UNIX AND NOT APPLE)
	list(APPEND LINK_DEPS stdc++fs dl)  # Link against stdc++fs for Linux
endif()

create_component(media_reader 0.1.0 "${LINK_DEPS}")

