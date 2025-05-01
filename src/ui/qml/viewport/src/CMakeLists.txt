SET(LINK_DEPS
	CAF::core
	Imath::Imath
	Qt6::Core
	Qt6::Qml
	Qt6::Quick
	xstudio::module
	xstudio::playhead
	xstudio::ui::opengl::viewport
	xstudio::ui::viewport
	xstudio::ui::qml::helper
	xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/hotkey_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/qml_viewport.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/qml_viewport_renderer.hpp"
)

find_package(Imath)

create_qml_component(viewport ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
