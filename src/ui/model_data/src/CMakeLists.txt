SET(LINK_DEPS
	${CAF_LIBRARY_core}
	xstudio::json_store
	xstudio::global_store
    xstudio::utility
	xstudio::ui::base
)

create_component_with_alias(ui_model_data xstudio::ui::model_data 0.1.0 "${LINK_DEPS}")