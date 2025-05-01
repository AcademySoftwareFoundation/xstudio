include(CTest)

SET(LINK_DEPS
	xstudio::module
	xstudio::global
	CAF::core
)

create_tests("${LINK_DEPS}")

