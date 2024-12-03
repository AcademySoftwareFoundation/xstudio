SET(LINK_DEPS
		${CAF_LIBRARY_core}
		Qt5::Core
		xstudio::ui::qml::helper
		xstudio::ui::qml::session
		xstudio::conform
		xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/conform_ui.hpp"
)

create_qml_component(conform 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
