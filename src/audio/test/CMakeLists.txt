include(CTest)

SET(LINK_DEPS
	xstudio::audio_output
	xstudio::playhead
	xstudio::global
    asound
)

create_tests("${LINK_DEPS}")

