SET(LINK_DEPS
	xstudio::json_store
	xstudio::utility
	xstudio::global_store
	xstudio::ui::base
	caf::core
)

create_component(module 0.1.0 "${LINK_DEPS}")
