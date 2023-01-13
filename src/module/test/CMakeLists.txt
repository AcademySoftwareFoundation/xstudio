include(CTest)

SET(LINK_DEPS
	xstudio::module
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")

