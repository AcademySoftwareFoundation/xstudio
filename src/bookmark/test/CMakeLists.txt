include(CTest)

SET(LINK_DEPS
	xstudio::bookmark
	xstudio::utility
	xstudio::broadcast
)

create_tests("${LINK_DEPS}")
