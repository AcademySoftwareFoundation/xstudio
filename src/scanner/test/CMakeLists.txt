include(CTest)

SET(LINK_DEPS
	xstudio::scanner
)

create_tests("${LINK_DEPS}")

