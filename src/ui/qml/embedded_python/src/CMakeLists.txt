SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::ui::qml::helper
	xstudio::utility
	xstudio::global_store
)

SET(EXTRAMOC
)

create_qml_component(embedded_python 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
