SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::ui::qml::helper
	xstudio::ui::qt::viewport_widget
	xstudio::utility
	xstudio::session
)

SET(EXTRAMOC
)

create_qml_component(studio 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
