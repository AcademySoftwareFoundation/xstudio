SET(LINK_DEPS
	xstudio::global_store
	caf::core
)

if(WIN32)
	#list(APPEND LINK_DEPS ghc_filesystem)  # Link against the MSVSLX implementation for Windows
elseif(UNIX)
	list(APPEND LINK_DEPS stdc++fs dl)  # Link against stdc++fs for Linux
endif()

create_component(media_metadata 0.1.0 "${LINK_DEPS}")
