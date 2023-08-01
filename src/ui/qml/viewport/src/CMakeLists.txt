SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Imath::Imath
	Qt5::Core
	Qt5::Qml
	Qt5::Quick
	Qt5::Widgets
	xstudio::module
	xstudio::ui::opengl::viewport
	xstudio::ui::qt::viewport_widget
	xstudio::ui::viewport
	xstudio::ui::qml::helper
	xstudio::ui::qml::playhead
	xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/hotkey_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/qml_viewport.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/qml_viewport_renderer.hpp"
)

find_package(Imath)

create_qml_component(viewport 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
