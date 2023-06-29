SET(LINK_DEPS
	${CAF_LIBRARY_core}
	Qt5::Core
	xstudio::audio_output
	xstudio::ui::qml::helper
	xstudio::utility
)

# SET(EXTRAMOC
# 	"${ROOT_DIR}/include/xstudio/ui/qml/playlist_selection_ui.hpp"
# )

create_qml_component(playhead 0.1.0 "${LINK_DEPS}" "${EXTRAMOC}")
