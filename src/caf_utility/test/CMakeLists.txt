include(CTest)

SET(LINK_DEPS
	xstudio::caf_utility
)

create_tests("${LINK_DEPS}")


