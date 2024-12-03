include(CTest)

SET(LINK_DEPS
	xstudio::broadcast
	xstudio::media
	caf::core
)

create_tests("${LINK_DEPS}")

