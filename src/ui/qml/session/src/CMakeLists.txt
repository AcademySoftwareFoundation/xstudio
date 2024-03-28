SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	Qt5::Test
	xstudio::ui::qml::helper
	xstudio::timeline
	xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/session_model_ui.hpp"
	"${ROOT_DIR}/include/xstudio/ui/qml/caf_response_ui.hpp"
)

create_qml_component(session 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
