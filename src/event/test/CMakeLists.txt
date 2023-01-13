include(CTest)

SET(LINK_DEPS
	xstudio::event
)

create_tests("${LINK_DEPS}")


