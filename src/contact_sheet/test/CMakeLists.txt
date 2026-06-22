include(CTest)

SET(LINK_DEPS
	xstudio::contact_sheet
	xstudio::audio_output
	xstudio::playlist
	xstudio::global
)

create_tests("${LINK_DEPS}")
