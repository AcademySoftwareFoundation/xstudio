include(CTest)

SET(LINK_DEPS
	xstudio::thumbnail
	xstudio::utility
	xstudio::media
	caf::core
)

create_tests("${LINK_DEPS}")
