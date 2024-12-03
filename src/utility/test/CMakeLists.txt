include(CTest)

SET(LINK_DEPS
	xstudio::bookmark
	caf::core
	xstudio::utility
)

create_tests("${LINK_DEPS}")


