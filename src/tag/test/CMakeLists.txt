include(CTest)

SET(LINK_DEPS
	xstudio::tag
	caf::core
)

create_tests("${LINK_DEPS}")
