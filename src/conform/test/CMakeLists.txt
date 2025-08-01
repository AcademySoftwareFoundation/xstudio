include(CTest)

SET(LINK_DEPS
	xstudio::conform
)

create_tests("${LINK_DEPS}")
