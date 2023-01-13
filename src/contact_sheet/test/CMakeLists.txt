include(CTest)

SET(LINK_DEPS
	xstudio::contact_sheet
	xstudio::audio_output
	xstudio::playlist
)

create_tests("${LINK_DEPS}")
