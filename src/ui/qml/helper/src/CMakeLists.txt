SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	Qt5::Qml
	xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/thumbnail_provider_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/shotgun_provider_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/json_tree_model_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/model_data_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/module_data_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/snapshot_model_ui.hpp"
)

create_qml_component(helper 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
