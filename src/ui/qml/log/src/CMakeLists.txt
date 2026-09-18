SET(LINK_DEPS
	CAF::core
	Qt6::Core
	xstudio::ui::qml::helper
	xstudio::utility
)

SET(EXTRAMOC
)

create_qml_component(log ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
