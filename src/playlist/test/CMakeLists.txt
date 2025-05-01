include(CTest)

SET(LINK_DEPS
	xstudio::playlist
	xstudio::global
	CAF::core
)

create_tests("${LINK_DEPS}")
