SET(LINK_DEPS
	CAF::core
	Qt6::Core
	xstudio::json_store
	xstudio::ui::qml::helper
)

SET(EXTRAMOC
)

create_qml_component(json_store ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
