include(CTest)

SET(LINK_DEPS
	xstudio::broadcast
	caf::core
)

create_tests("${LINK_DEPS}")

