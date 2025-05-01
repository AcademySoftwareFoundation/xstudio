SET(LINK_DEPS
	xstudio::utility
	xstudio::json_store
	xstudio::global_store
	xstudio::timeline
	CAF::core
)

create_component(conform ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
