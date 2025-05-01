SET(LINK_DEPS
	CAF::core
	Qt6::Core
	xstudio::ui::qml::helper
	xstudio::utility
	xstudio::global_store
)

SET(EXTRAMOC
)

create_qml_component(embedded_python ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
