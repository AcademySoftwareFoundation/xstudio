SET(LINK_DEPS
	xstudio::global_store
	CAF::core
	xstudio::media
)

if(UNIX)
	list(APPEND LINK_DEPS dl)
	if (NOT APPLE)
		list(APPEND LINK_DEPS stdc++fs)
	endif()
endif()

create_component(media_hook ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")