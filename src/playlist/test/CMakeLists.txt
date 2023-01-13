include(CTest)

SET(LINK_DEPS
	xstudio::playlist
	xstudio::global
	caf::core
)

create_tests("${LINK_DEPS}")
