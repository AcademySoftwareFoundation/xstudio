include(CTest)

SET(LINK_DEPS
	xstudio::thumbnail
	xstudio::utility
	xstudio::media
	CAF::core
)

create_tests("${LINK_DEPS}")
