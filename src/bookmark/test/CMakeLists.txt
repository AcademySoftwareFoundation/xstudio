include(CTest)

SET(LINK_DEPS
	xstudio::bookmark
	xstudio::utility
)

create_tests("${LINK_DEPS}")
