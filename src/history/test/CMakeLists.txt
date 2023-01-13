include(CTest)

SET(LINK_DEPS
	xstudio::utility
)

create_tests("${LINK_DEPS}")

