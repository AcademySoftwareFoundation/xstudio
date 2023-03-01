SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::global_store
	xstudio::ui::qml::helper
	xstudio::ui::qml::json_tree_model
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/global_store_model_ui.hpp"
)

create_qml_component(global_store 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
