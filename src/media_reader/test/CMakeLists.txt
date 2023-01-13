include(CTest)

SET(LINK_DEPS
	xstudio::playhead
	xstudio::media_reader
	xstudio::colour_pipeline
	caf::core
)

create_tests("${LINK_DEPS}")
