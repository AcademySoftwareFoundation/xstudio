SET(LINK_DEPS
	xstudio::global_store
	caf::core
	xstudio::media
)

if(UNIX)
	list(APPEND LINK_DEPS stdc++fs dl)
endif()

create_component(media_hook 0.1.0 "${LINK_DEPS}")

