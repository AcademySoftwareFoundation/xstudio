SET(LINK_DEPS
		CAF::core
		Qt6::Core
		xstudio::ui::qml::helper
		xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/bookmark_model_ui.hpp"
)

create_qml_component(bookmark ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
