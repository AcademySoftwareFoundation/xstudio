include(CTest)

SET(LINK_DEPS
	xstudio::playhead
	xstudio::audio_output
	xstudio::module
	xstudio::global
)

create_tests("${LINK_DEPS}")
