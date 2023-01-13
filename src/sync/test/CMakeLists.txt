include(CTest)

SET(LINK_DEPS
	xstudio::sync
	caf::core
)

create_tests("${LINK_DEPS}")
