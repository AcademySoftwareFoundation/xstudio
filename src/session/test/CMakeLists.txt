include(CTest)

SET(LINK_DEPS
	xstudio::session
	xstudio::global
)

create_tests("${LINK_DEPS}")

