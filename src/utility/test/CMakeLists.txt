include(CTest)

SET(LINK_DEPS
	xstudio::bookmark
	caf::core
)

create_tests("${LINK_DEPS}")


