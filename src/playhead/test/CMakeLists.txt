include(CTest)

SET(LINK_DEPS
	xstudio::playhead
	xstudio::audio_output
	xstudio::module
	xstudio::global
	xstudio::ui::viewport	
)

create_tests("${LINK_DEPS}")
