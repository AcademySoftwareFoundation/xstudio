include(CTest)

SET(LINK_DEPS
	xstudio::global
	CAF::core
)

create_tests("${LINK_DEPS}")


