SET(LINK_DEPS
	CAF::core
	Qt6::Core
	xstudio::global_store
	xstudio::ui::qml::helper
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/global_store_model_ui.hpp"
)

create_qml_component(global_store ${XSTUDIO_GLOBAL_VERSION} "${LINK_DEPS}" "${EXTRAMOC}")
