SET(LINK_DEPS
	${CAF_LIBRARY_core}
    xstudio::utility
)

create_component_with_alias(ui_model_data xstudio::ui::model_data 0.1.0 "${LINK_DEPS}")