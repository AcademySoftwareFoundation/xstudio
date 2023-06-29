SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::media
	xstudio::ui::qml::helper
	xstudio::utility
)

SET(EXTRAMOC
	"${ROOT_DIR}/include/xstudio/ui/qml/thumbnail_ui.hpp"
)

create_qml_component(media 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
