include(CTest)

SET(LINK_DEPS
	xstudio::bookmark
	CAF::core
	xstudio::utility
)

create_tests("${LINK_DEPS}")


