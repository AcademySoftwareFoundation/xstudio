include(CTest)

SET(LINK_DEPS
	CAF::core
	xstudio::global
	xstudio::playlist
	xstudio::timeline
)

create_tests("${LINK_DEPS}")
