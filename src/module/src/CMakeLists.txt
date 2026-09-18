SET(LINK_DEPS
	xstudio::json_store
	xstudio::utility
	xstudio::global_store
	xstudio::ui::base
	CAF::core
)

create_component(module ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")
