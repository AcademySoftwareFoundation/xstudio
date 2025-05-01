SET(LINK_DEPS
	CAF::core
	Qt6::Core
	Qt6::OpenGLWidgets
	xstudio::ui::qml::bookmark
	xstudio::ui::qml::conform
	xstudio::ui::qml::embedded_python
	xstudio::ui::qml::global_store
	xstudio::ui::qml::helper
	xstudio::ui::qml::log
	xstudio::ui::qml::session
	xstudio::ui::qml::viewport
	xstudio::ui::qt::viewport_widget
	xstudio::utility
	xstudio::session
)

SET(EXTRAMOC
)

create_qml_component(studio ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
