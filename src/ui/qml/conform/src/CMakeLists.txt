SET(LINK_DEPS
		CAF::core
		Qt6::Core
		xstudio::ui::qml::helper
		xstudio::ui::qml::session
		xstudio::conform
		xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/conform_ui.hpp"
)

create_qml_component(conform ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
