SET(LINK_DEPS
	CAF::core
	xstudio::json_store
	xstudio::global_store
    xstudio::utility
	xstudio::ui::base
)

create_component_with_alias(ui_model_data xstudio::ui::model_data ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}")