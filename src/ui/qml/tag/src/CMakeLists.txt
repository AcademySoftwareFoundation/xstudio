SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::ui::qml::helper
)

SET(EXTRAMOC
)

create_qml_component(tag 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
