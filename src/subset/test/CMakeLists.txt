include(CTest)

SET(LINK_DEPS
	xstudio::subset
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")
