include(CTest)

SET(LINK_DEPS
	xstudio::utility
	xstudio::media
)

create_tests("${LINK_DEPS}")

