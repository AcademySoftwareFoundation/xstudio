SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::json_store
	xstudio::ui::qml::helper
)

SET(EXTRAMOC
)

create_qml_component(json_store 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
