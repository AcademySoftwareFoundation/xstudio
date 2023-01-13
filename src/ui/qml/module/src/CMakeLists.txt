
SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::module
	xstudio::ui::qml::helper
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/module_menu_ui.hpp"
)

create_qml_component(module 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
