include(CTest)

SET(LINK_DEPS
	xstudio::session
)

create_tests("${LINK_DEPS}")

