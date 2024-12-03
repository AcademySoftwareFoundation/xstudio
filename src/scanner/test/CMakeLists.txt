include(CTest)

SET(LINK_DEPS
	xstudio::scanner
	xstudio::media
)

create_tests("${LINK_DEPS}")

