include(CTest)

SET(LINK_DEPS
	xstudio::media
	xstudio::media_hook
	caf::core
)

create_tests("${LINK_DEPS}")



