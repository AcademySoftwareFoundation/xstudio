include(CTest)

SET(LINK_DEPS
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")


