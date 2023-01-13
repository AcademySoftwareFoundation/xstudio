SET(LINK_DEPS
	xstudio::media
	xstudio::media_cache
	xstudio::global_store
	xstudio::broadcast
	caf::core
	stdc++fs
	dl
)

create_component(media_reader 0.1.0 "${LINK_DEPS}")

