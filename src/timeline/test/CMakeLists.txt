include(CTest)

SET(LINK_DEPS
	caf::core
	xstudio::global
	xstudio::playlist
	xstudio::timeline
)

create_tests("${LINK_DEPS}")
