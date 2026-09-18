include(CTest)

SET(LINK_DEPS
	xstudio::media
	xstudio::media_hook
	CAF::core
)

create_tests("${LINK_DEPS}")



