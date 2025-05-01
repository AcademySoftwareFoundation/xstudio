SET(LINK_DEPS
	xstudio::global_store
	CAF::core
)

create_component(data_source ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
