include(CTest)

SET(LINK_DEPS
	xstudio::subset
	xstudio::global
	CAF::core
)

create_tests("${LINK_DEPS}")
