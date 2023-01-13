include(CTest)

SET(LINK_DEPS
	xstudio::thumbnail
	xstudio::utility
	caf::core
)

create_tests("${LINK_DEPS}")
